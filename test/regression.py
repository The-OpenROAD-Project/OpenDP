import os
import sys
import subprocess as sp
import shlex

useValgrind = False 
useScreen = False 

test_path = os.path.realpath(__file__)
test_dir = os.path.dirname(test_path)
opendp_dir = os.path.dirname(test_path)
openroad_dir = os.path.dirname(os.path.dirname(os.path.dirname(opendp_dir)))
prog = os.path.join(openroad_dir, "build", "src", "openroad")

def ExecuteCommand( cmd, log="" ):
  print( "CMD: " + cmd )
  # print( shlex.split(cmd) )
  if log == "":
    sp.call( shlex.split(cmd), shell=False, stdout=None )
  else:
    # print( "LOG: " + log )
    f = open(log, "w")
    p = sp.Popen( shlex.split(cmd), stdout=f, stderr=f)
    p.wait()
    f.close()

    #f = open(log, "r")
    #print( f.read() )
    #f.close()

# threshold setting
def DiffVar(gVar, tVar, threshold):
  # smaller is the better
  if gVar >= tVar:
    return True

  if abs(gVar - tVar) / abs(gVar) <= threshold:
    return True
  else:
    return False

def MustSmaller(gVar, tVar, threshold):
  return True if gVar >= tVar else False

def DiffExactVar(gVar, tVar):
  return True if gVar == tVar else False

def SimpleGoldenCompare(orig, ok):
  f = open(orig, "r")
  origCont = f.read().split("\n")
  f.close()

  o = open(ok, "r")
  goldenCont = o.read().split("\n")
  o.close()

  gLegal = float(goldenCont[0].split(": ")[-1])
  gSumDisplace = float(goldenCont[2].split(": ")[-1])
  
  tLegal = float(origCont[0].split(": ")[-1])
  tSumDisplace = float(origCont[2].split(": ")[-1])
 
  if DiffExactVar(gLegal, tLegal) == False:
    print("Legality is different! %d %d" %(gLegal, tLegal))
    sys.exit(1)

  if MustSmaller(gSumDisplace, tSumDisplace, 5) == False:
    print("Sum_Displacement has more than 5 percents diff: %g %g" %(gSumDisplace, tSumDisplace))
    sys.exit(1)

  print("  " + ok + " passed!")

def Run(mode, binaryLoc, curList):
  # regression for TD test cases
  for curCase in curList:
    ExecuteCommand("rm -rf %s/*.rpt" % (curCase))
    ExecuteCommand("rm -rf %s/*.log" % (curCase))
  
  for curCase in curList:
    print ( "Access " + curCase + ":")
    for cFile in os.listdir(curCase):
      if cFile.endswith(".tcl") == False:
        continue
      print ( "  " + cFile )
      cmd = "%s %s/%s" % (binaryLoc, curCase, cFile)
      log = "%s/%s.log" % (curCase, cFile)
      if useValgrind: 
        cmd = "valgrind --log-fd=1 %s/%s" % (binaryLoc, curCase, cFile)
        log = "%s/%s_mem_check.log" % (curCase, cFile)
        ExecuteCommand(cmd, log)
      elif useScreen:
        scName = "%s_%s" %(curCase, cFile)
        ExecuteCommand("screen -dmS %s bash" %(scName))
        ExecuteCommand("screen -S %s -X stuff \"%s \n\"" % (scName, cmd))
      else:
        ExecuteCommand(cmd, log)

    print("Compare with golden: ")
    for cFile in os.listdir(curCase):
      if cFile.endswith(".ok") == False:
        continue
      rptFile = "%s/%s" % (curCase, cFile[:-3])
      goldenFile = "%s/%s" % (curCase, cFile)
      
      if mode == "simple":
        SimpleGoldenCompare(rptFile, goldenFile)



if len(sys.argv) <= 1:
  print("Usage: python regression.py run")
  print("Usage: python regression.py run openroad")
  print("Usage: python regression.py get")
  sys.exit(0)

dirList = os.listdir(".")
simpleList = []
lowUtilList = []
fenceList = []
multiList = []
iccadList = []
openroadList = []
for cdir in sorted(dirList):
  if os.path.isdir(cdir) == False:
    continue

  if cdir.startswith("or-"):
    openroadList.append(cdir)
  elif "multi-height-test" in cdir:
    multiList.append(cdir)
  elif "simple-test" in cdir:
    simpleList.append(cdir)
  elif "low-util-test" in cdir:
    lowUtilList.append(cdir)
  #elif "fence-test" in cdir:
  #  fenceList.append(cdir)

if sys.argv[1] == "run":
  if len(sys.argv) >= 3 and sys.argv[2] == "openroad":
    #Run("simple", prog, openroadList)
    Run("simple", "./openroad", openroadList)
  else:
    Run("simple", "./opendp", simpleList)
    Run("simple", "./opendp", fenceList)
    Run("simple", "./opendp", multiList)
    Run("simple", "./opendp", lowUtilList)
elif sys.argv[1] == "get":
  ExecuteCommand("watch -n 3 \"grep -r '' *-test*/exp/*.rpt\"")
