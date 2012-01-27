edify = require("./edify/lib/edify")()
edify.language "coffee"
  lexer: "coffeescript"
  docco: "#"
  ignore: [ /^#!/, /^#\s+vim/ ]
edify.language "c"
  lexer: "c"
  divider: "/* --- EDIFY DIVIDER --- */"
  docco:
    start:  /^\s*\/\*\s*(.*)/
    end:    /^(.*)\*\//
    strip:  /^\s+\*\s*/
edify.parse "c", "code", ".", /attendant\.c$/
edify.stencil /\/.*.c$/, "stencil/docco.stencil"
edify.tasks task
