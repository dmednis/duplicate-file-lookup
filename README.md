# duplicate-file-lookup

Utility for duplicate file lookup in the current working directory.  
By default looks for duplicate files by name and size.  
usage: `duplicate-file-lookup [-m] [-d] [-h]`  
Detailed description of flag usage:  
* -m    Uses modified time in conjunction with name and size for duplicate comparison.  
* -d    Uses only MD5 content hash for duplicate comparison.  
* -d -m   Uses modified time and MD5 content hash in conjunction with name and size for duplicate comparison.  
* -h    Displays this help text.  

### Dependencies
* `libssl-dev`

### Building
`gcc -Wall -o duplicate-file-lookup main.c -lcrypto`
