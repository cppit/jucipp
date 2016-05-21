# juCi++ API doc

## Prerequisites:
 * doxygen
 * plantuml 
  * install via apt-get or download from http://plantuml.com/
  * see also http://plantuml.com/starting.html
  * if downloaded either copy the jar file to /usr/bin or set the environment variable PLANTUML_PATH to point to the path containing the jar file)

## How to build the API doc:
```sh
mkdir jucipp/build
cd jucipp/build
cmake ..
make doc
```

## Where is the generated API documentation
Open jucipp/build/src/html/index.html
