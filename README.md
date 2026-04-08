A fully functional text editor in terminal mimicking [VIM](https://www.vim.org/). To run, execute ./vm executable under src

- follows MVC architecture
- Uses inheritance and polymorphism for smooth switch between modes (insert, normal, command, replace, search), implements strategy pattern
- No raw pointer manipulation, entirely handled by RAII with smart pointers
- Implements a fully functional compiler
  - Implements a finite state machine in the scanner, handles same string in different contexts accordingly (ex. "Hello World!" and // "Hello World!")
  - Implements an abstract syntax tree in the context-aware parser
  - Enables syntax highlighting and brace matching
 
Note that a typical vim command has the shape of multiplier1 operation multiplier2 motion. For example, 2d3w means "delete the next 3 words after the cursor, perform 2 times".
Command parsing is done in handleKey(key), command execution is done in executeCommand()
