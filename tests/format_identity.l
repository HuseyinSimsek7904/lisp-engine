(include "core/assert.l")

;; This function should return its argument, hence the identity function.
;; (Assuming builtin and type names are not redefined)
;; Allows for testing whether the producer formatting is working or not.
;; lisp-obj x -> format "%l" -> str -> parse & eval -> lisp-obj x
(setg id (fn (x) (eval (head (parse-lisp (format "%L" x))))))

(setg assert-id (fn (x) (assert-eq x (id x))))

(assert-id nil)

(assert-id true)
(assert-id false)

(assert-id nil-t)
(assert-id type-t)
(assert-id error-t)
(assert-id str-t)
(assert-id int64-t)
(assert-id bool-t)
(assert-id sym-t)
(assert-id builtin-t)
(assert-id fn-t)
(assert-id list-t)

(assert-id (error 123))
(assert-id (error "hello"))
(assert-id (error include))

(assert-id "abc")
(assert-id "print")
(assert-id "include")
(assert-id "(eval \"Terrible code...\")")
(assert-id "\x00\x0a\x00")

(assert-id -9223372036854775808)
(assert-id -9223372036854775807)
(assert-id -123)
(assert-id -1)
(assert-id 0)
(assert-id 1)
(assert-id 123)
(assert-id 9223372036854775807)

(assert-id print)
(assert-id format)
(assert-id <)

(assert-id include)
(assert-id >)
(assert-id !)
(assert-id (fn () nil))
(assert-id (fn (a) nil))
(assert-id (fn (a b) nil))
(assert-id (fn (a b c) nil))
(assert-id (fn ((rest r)) nil))
(assert-id (fn (a (rest r)) nil))
(assert-id (fn (a b (rest r)) nil))
(assert-id (fn (a b c (rest r)) nil))

(assert-id (list))
(assert-id (list 1))
(assert-id (list 1 2))
(assert-id (list 1 2 3))
(assert-id '())
(assert-id '(a))
(assert-id '(a b))
(assert-id '(a (b c) d))
