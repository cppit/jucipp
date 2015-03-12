#!/usr/bin/python
#plugin handler
import juci_to_python_api as juci, os, glob

def loadplugins():
    cwd = os.getcwd()
    plugin_files = glob.glob(cwd+"/plugins/*.py")
    for current_file in plugin_files:
        juci.initPlugin(current_file)

loadplugins()
