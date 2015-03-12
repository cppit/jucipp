#!/usr/bin/python
#snippet plugin
import juci_to_python_api
import os
import glob

def loadplugins():
    cwd = os.getcwd()
    plugin_files = glob.glob(cwd+"/plugins/*.py")
    for current_file in plugin_files:
        (filepath, filename) = os.path.split(current_file)
        (name, extension) = filename.split(".")
        if filename != "plugins.py":
            print(filename+" ("+current_file+") loading...")
            #juci_to_python_api.loadPlugin(current_file)
#            juci_to_python_api.addMenuElement(name.capitalize())#, current_file)
            juci_to_python_api.initPlugin(current_file)
            print(filename+" loaded...")

loadplugins()
