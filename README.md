# mel (last version 0.2.0, 02/01/2025)

mel (mini embedded light editor for linux, especially build for linux and unix, but definitely it'll work on other platforms with some exceptions, may be) is a terminal based text editor written in C from scratch, trying to be very minimalistic and dependency independent (it's not even using **curses**) with syntax highlighting in support of the following languages: C, C++, Java, Bash, Mshell, Python, PHP, JavaScript, JSON, XML, SQL, Ruby, Go. It was created specifically for resource-intensive environments like small containers, high-load AMI's (aws), small POD's in kubernetes for various cloud platforms (tested on AWS, GCP, OCI) and IOT solutions (embedded C) on various ARM64 bit architectures and different CPU's (like Convex, etc.), as well as on standard Linux and other platforms based on different CPU's (Intel, AMD, etc.) The code is distributed under GNU license ans is available for anyone to incorporate and implement in solutions that do not violate the GNU license.


## Installation

### Compiling
```
git clone https://github.com/igor101964/mel.git
cd mel/
make
make install
```
### Downloading executable
Download it from (https://github.com/igor101964/mel), then
```
sudo mv mel /usr/local/bin/
sudo mv mel /bin/
sudo mv mel /usr/bin/
sudo chmod +x /usr/local/bin/mel
```
### Uninstall
```
sudo rm /usr/local/bin/mel
sudo rm mel /bin/mel/
sudo rm mel /usr/bin/mel
```


## Usage
```
mel [file_name]
mel -h | --help
mel -v | --version
mel -e | --extension <file_extension> <file_name>
mel -t | --use-tabs [file_name]
```

## Keybindings
The key combinations chosen here are the ones that fit the best for me.
```
Ctrl-Q : Exit
Ctrl-F : Search text (ESC, arrows and enter to interact once searching)
Ctrl-G : Go to line Number
Ctrl-B   Hide/Show line numbering
Ctrl-S : Save
Ctrl-E : Flip line upwards
Ctrl-D : Flip line downwards
Ctrl-C : Copy line
Ctrl-X : Cut line
Ctrl-V : Paste line
Ctrl-Z : Undo
Ctrl-Y : Redo
Ctrl-P : Pause tte (type "fg" to resume)
Ctrl-W : Retrieve Ollama LLM response
Ctrl-H : Toggle help screen
```

## License
```
Supports highlighting for C,C++,Java,Bash,Mshell,Python,PHP,Javascript,JSON,XML,SQL,Ruby,Go.
License: Public domain libre software GPL3,v.0.2.0, 2025
Initial coding: Igor Lukyanov, igor.lukyanov@appservgrid.com
For now, usage of UTF-8 is recommended.
```
	
## Current supported languages
```
* C (`*.c`, `*.h`)
* C++ (`*.cpp`, `*.hpp`, `*.cc`)
* Java (`*.java`)
* Bash (`*.sh`)
* Mshell (`*.ms`)
* Python (`*.py`)
* PHP (`*.php`)
* JavaScript (`*.js`, `*.jsx`)
* JSON (`*.json`, `*.jsonp`)
* XML (partially) (`*.xml`)
* SQL (`*.sql`)
* Ruby (`*.rb`)
* Go (`*.go`)
```
