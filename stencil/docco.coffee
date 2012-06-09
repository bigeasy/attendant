module.exports =
  initializer: ->
    @edify = require("edify").create()
    @edify.language
      lexer: "c"
      divider: "/* --- EDIFY DIVIDER --- */"
      docco:
        start:  /^\s*\/\*\s*(.*)/
        end:    /^(.*)\*\//
        strip:  /^\s+\*\s*/
