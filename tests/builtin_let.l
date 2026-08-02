(include "core/assert.l")

;; `let` structure: (let ([(SYMBOL VALUE)...]) BODY...)

;; Wrong number of arguments.
(assert-eq (let)
           (error `(too-few-args-to-builtin ~let 0 2)))
(assert-eq (let 1)
           (error `(too-few-args-to-builtin ~let 1 2)))

;; Wrong type of argument.
(assert-eq (let 1 nil)
           (error `(wrong-type-arg-to-builtin ~let 1 ~list-t)))
(assert-eq (let (12) nil)
           (error `(wrong-type-arg-to-builtin ~let 12 ~list-t)))
(assert-eq (let ((1 2 3)) nil)
           (error `(wrong-type-arg-to-builtin ~let (1 2 3)
                        "`let` binding list must be a list of key-value pairs")))
(assert-eq (let ((1 2)) nil)
           (error `(wrong-type-arg-to-builtin ~let 1 ~sym-t)))
(assert-eq (let ((1 2)) nil)
           (error `(wrong-type-arg-to-builtin ~let 1 ~sym-t)))

;; Redefinition of variable in same `let`.
(assert-eq (let ((a 10) (a 15)) nil)
           (error `(redefined-symbol a)))

;; Functionality test.
(assert-eq (let () "hello") "hello")
(assert-eq (let ((a 2)) a) 2)
(assert-eq (let ((a 2) (b 3)) a b) 3)
