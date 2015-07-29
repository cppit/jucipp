#!/usr/bin/python
import juci_to_python_api as juci
import glob

def loadplugins():
    plugin_files = glob.glob("../plugins/*.py")
    for current_file in plugin_files:
        juci.initPlugin(current_file)
loadplugins()

