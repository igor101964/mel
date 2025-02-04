# mel (last version 0.2.0, 02/01/2025)

mel (mini embedded light editor for linux, especially build for linux and unix, but definitely it'll work on other platforms with some exceptions, may be) is a terminal based text editor written in C from scratch, trying to be very minimalistic and dependency independent (it's not even using **curses**) with syntax highlighting in support of the following languages: C, C++, Java, Bash, Mshell, Python, PHP, JavaScript, JSON, XML, SQL, Ruby, Go. It was created specifically for resource-intensive environments like small containers, high-load AMI's (aws), small POD's in kubernetes for various cloud platforms (tested on AWS, GCP, OCI) and IOT solutions (embedded C) on various ARM64 bit architectures and different CPU's (like Convex, etc.), as well as on standard Linux and other platforms based on different CPU's (Intel, AMD, etc.) The code is distributed under GNU license ans is available for anyone to incorporate and implement in solutions that do not violate the GNU license.


## Installation

### Compiling
```
git clone https://github.com/igor101964/mel.git
cd mel/
make
```
### Downloading executable
Download it from (https://github.com/igor101964/mel), then
```
sudo mv mel /usr/local/bin/
sudo mv mel /bin/
sudo mv mel /usr/bin/
sudo chmod +x /usr/local/bin/mel
sudo chmod +x /usr/bin/mel
sudo chmod +x /bin/mel
```
### Uninstall
```
sudo rm /usr/local/bin/mel
sudo rm /bin/mel
sudo rm /usr/bin/mel
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
Ctrl-Q    :   Exit, 3 times click Ctrl-Q if file was changed without saving
Ctrl-S    :   Save, requires input of file name, if file didn't exist
Ctrl-F    :   Search by pattern, Esc - exit from Search, Enter and Arrows to interact searching
Ctrl-N    :   Forward Search by pattern after Ctrl-F. Esc - exit from Search, works after Ctrl-F only
Ctrl-R    :   Backward Search by pattern after Ctrl-F. Esc - exit from Search, works after Ctrl-F only
Ctrl-J    :   Global replacement of character combinations, Input Search and Replace patterns, Esc to cancel, Enter to input
Ctrl-G    :   Go to line Number, requires input the line number
Ctrl-B    :   Hide/Show line numbering
Ctrl-E    :   Flip line upwards
Ctrl-D    :   Flip line downwards
Ctrl-C    :   Copy line
Ctrl-X    :   Cut line
Ctrl-V    :   Paste line
Ctrl-Z    :   Undo
Ctrl-Y    :   Redo
Ctrl-P    :   Pause mel (type "fg" to resume)
Ctrl-W    :   Retrieve Ollama LLM response
Ctrl-H    :   Toggle this help screen
Home      :   Move the cursor to the beginning of the line
End       :   Move cursor to end of line
PgUp      :   Up page scroll
PgDn      :   Down page scroll
Up        :   Move cursor up one position
Down      :   Move cursor down one position
Left      :   Move cursor left one position
Right     :   Move cursor right one position
Backspace :   Delete character
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
