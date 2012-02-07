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
edify.parse "c", "code", ".", /attendant[^\/]*\.[hc]$/
edify.parse "c", "code", ".", /relay[^\/]*\.c$/
edify.stencil /\/.*.[ch]$/, "stencil/docco.stencil"
edify.tasks task
