(include "core/assert.l")

;; `read-line` structure: (read-line PROMPT)

;; Wrong number of arguments.
(assert-eq (read-file)
           (error `(wrong-number-of-args-to-builtin ~read-file 0 1)))
(assert-eq (read-file 1 2)
           (error `(wrong-number-of-args-to-builtin ~read-file 2 1)))

;; Wrong type of argument.
(assert-eq (read-file 1)
           (error `(wrong-type-arg-to-builtin ~read-file 1 ~str-t)))

;; Error as argument.
(assert-eq (read-file (error "something"))
           (error `(wrong-type-arg-to-builtin ~read-file ~(error "something") ~str-t)))

;; Cannot read file.
(assert-eq (read-file "abcd1234.xyz")
           (error `(could-not-read-file "abcd1234.xyz")))

;; Reading an example file.
(assert-eq (read-file "examples/hello_world.l") "(print \"Hello, world!\")\n")
