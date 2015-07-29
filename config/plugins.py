#!/usr/bin/python
#plugin handler
import sys, os, glob
cwd = os.getcwd()
sys.path.append(cwd+"/lib")

import juci_to_python_api as juci
def loadplugins():
    plugin_files = glob.glob(cwd+"/plugins/*.py")
    for current_file in plugin_files:
        juci.initPlugin(current_file)

loadplugins()
