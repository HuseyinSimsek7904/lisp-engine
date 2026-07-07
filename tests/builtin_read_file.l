(include "core/assert.l")

;; `read-file` structure: (read-file FILE)

;; Wrong number of arguments.
(assert-eq (read-file) (error (list (quote wrong-number-of-args-to-builtin) read-file 0 1)))
(assert-eq (read-file 1 2) (error (list (quote wrong-number-of-args-to-builtin) read-file 2 1)))

;; Wrong type of argument.
(assert-eq (read-file 1) (error (list (quote wrong-type-arg-to-builtin) read-file 1 str-t)))

;; Error as argument.
(assert-eq (read-file (error "something")) (error (list (quote wrong-type-arg-to-builtin) read-file (error "something") str-t)))

;; Cannot read file.
(assert-eq (read-file "abcd1234.xyz") (error (list (quote could-not-read-file) "abcd1234.xyz")))

;; Reading an example file.
(assert-eq (read-file "examples/fibonacci.l") "(setg fib (fn (n) (if (= n 0) 0 (if (= n 1) 1 (+ (fib (- n 1)) (fib (- n 2)))))))\n")
