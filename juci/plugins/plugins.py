#!/usr/bin/python
#snippet plugin
import juci_to_python_api
import os, glob

cwd = os.getcwd()

plugins = glob.glob(cwd+"/*.py")
for current_file in plugins:
    (filepath, filename) = os.path.split(current_file)
    if filename != "plugins.py":
        juci_to_python_api.loadPlugin(filename)
        juci_to_python_api.addMenuElement(filename)
