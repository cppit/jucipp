#!/usr/bin/python
#snippet plugin
import juci_to_python_api

print("From Python: Snippet run")

snippets = {}
snippets["hoy"] = "hoyvalue"
snippets["for"] = """\
for(#int i=0; #i<#v.size(); #i++) {
   std::cout << v[i] << std::endl;
}
"""
snippets["if"] = """\
if(#) {
    # 
}
"""
snippets["ifelse"] = """\
if(#) {
    #
} else {
    #
}
"""
snippets["while"] = """\
while(#) {
    #
}
"""
snippets["main"] = """\
int main(int argc, char *argv[]) {
    //Do something
}
"""
snippets["hello"] = """\
#include <iostream>

int main(int argc, char *argv[]) {
    std::cout << "Hello, world! << std::endl;
}
"""
def getSnippet(word):    
    try:
        output = snippets[word]
    except KeyError:
        output = word
    return output

theWord=juci_to_python_api.getWord()
output=getSnippet(theWord)

print(theWord)
juci_to_python_api.replaceWord(output)
