(include "core/assert.l")

;; `=` structure: (= A B)

;; Wrong number of arguments.
(assert-eq (=)
           (error `(wrong-number-of-args-to-builtin ~= 0 2)))
(assert-eq (= 1 2 3)
           (error `(wrong-number-of-args-to-builtin ~= 3 2)))

;; Different type identity checks.
(assert-eq (= nil 4) false)
(assert-eq (= "hi" print) false)
(assert-eq (= '(1 2 3) 'abc) false)

;; nil-t
(assert-eq (= nil nil) true)
(assert-eq (= nil empty) false)

;; type-t
(assert-eq (= int64-t int64-t) true)
(assert-eq (= builtin-t builtin-t) true)

(assert-eq (= str-t list-t) false)
(assert-eq (= error-t fn-t) false)

;; error-t
(assert-eq (= (error nil) (error nil)) true)
(assert-eq (= (error "should be true") (error "should be true")) true)

(assert-eq (= (error "should not be true") (error "wrong")) false)
(assert-eq (= (error (quote (wrong))) (error '((wrong)))) false)

;; str-t
(assert-eq (= "abc" "abc") true)
(assert-eq (= "Hello, world!" "Hello, world!") true)

(assert-eq (= "abc" "def") false)
(assert-eq (= "123" "456") false)

;; int64-t
(assert-eq (= 0 0) true)
(assert-eq (= 123 123) true)
(assert-eq (= 123 234) false)
(assert-eq (= 123 -123) false)
(assert-eq (= 123 -234) false)

(assert-eq (= -123 -123) true)
(assert-eq (= -123 -234) false)

;; bool-t
(assert-eq (= false false) true)
(assert-eq (= true true) true)

(assert-eq (= false true) false)
(assert-eq (= true false) false)

;; sym-t
(assert-eq (= 'abc 'abc) true)
(assert-eq (= 'def 'def) true)

(assert-eq (= 'abc 'not-equal) false)
(assert-eq (= 'abc 'maybe-equal) false)

;; builtin-t
(assert-eq (= print print) true)
(assert-eq (= = =) true)
(assert-eq (= fn fn) true)
(assert-eq (= macro macro) true)

(assert-eq (= print fn) false)
(assert-eq (= fn macro) false)
(assert-eq (= macro fn) false)
(assert-eq (= = <) false)

;; fn-t
(assert-eq (= include include) true)
(assert-eq (= assert-eq assert-eq) true)

(assert-eq (= assert-eq include) false)
(assert-eq (= assert-eq assert-type) false)

;; list-t
(assert-eq (= nil nil) true)
(assert-eq (= '(1 2 3) '(1 2 3)) true)
(assert-eq (= '(1 '(2 3)) '(1 '(2 3))) true)

(assert-eq (= nil "a") false)
(assert-eq (= '(1 2 3) '(1 2 4)) false)
(assert-eq (= '(1 '(2 3)) '(1 '(2 4))) false)
