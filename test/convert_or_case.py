import os
import subprocess as sp

def ExecuteCommand(cmd):
  print(cmd)
  sp.call( cmd, shell=True )

def Run(mode, curList):
  for curCase in curList:
    copyCase = "or-%s" % (curCase)
    # copy testcases
    # ExecuteCommand("cp -r %s %s" % (curCase, copyCase))
    for cFile in os.listdir(copyCase):
      if cFile.endswith(".tcl") == False:
        continue
      print ( "%s/%s" %(copyCase, cFile ) )
      ModifyCont( copyCase, "%s/%s" % (copyCase, cFile) )


def ModifyCont(folderName, newFileName):
  f = open( newFileName, 'r')
  tclCont = f.read().split("\n")
  f.close()

  copyCont = tclCont

  for idx, curLine in enumerate(copyCont):
    if curLine.find('odp import_lef') is not -1:
      tclCont[idx] = curLine = curLine.replace('odp import_lef', 'read_lef')
    if curLine.find('odp import_def') is not -1:
      tclCont[idx] = curLine = curLine.replace('odp import_def', 'read_def')
    if curLine.find('odp init_opendp') is not -1:
      tclCont[idx] = curLine = "" 
    if curLine.find('odp legalize_place') is not -1:
      tclCont[idx] = curLine = "" 
    if curLine.find('set exp') is not -1:
      tclCont[idx] = curLine = "set exp " + folderName 
    
    if curLine.find('opendp_external odp') is not -1:
      tclCont[idx] = curLine = "set odp [opendp_external]" 
    elif curLine.find('odp') is not -1:
      tclCont[idx] = curLine = curLine.replace("odp", "$odp")
  
  f = open( newFileName, 'w')
  f.write( "\n".join(tclCont) )
  f.close()

dirList = os.listdir(".")
targetList = []
for cdir in sorted(dirList):
  if os.path.isdir(cdir) == False:
    continue
  
  if cdir.startswith("or-"):
    continue
  
  if "iccad17-test" in cdir:
    targetList.append(cdir)
  if "multi-height-test" in cdir:
    targetList.append(cdir)
  if "simple-test" in cdir:
    targetList.append(cdir)
  if "low-util-test" in cdir:
    targetList.append(cdir)
  if "fence-test" in cdir:
    targetList.append(cdir)


Run("", targetList)
