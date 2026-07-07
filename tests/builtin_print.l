(include "core/assert.l")

;; `print` structure: (print STR...)

;; Any amount of argument.
(assert-eq (print) nil)
(assert-eq (print "print test 1") nil)
(assert-eq (print "print test 2" "print test 3") nil)

;; Wrong type of argument.
(assert-eq (print 1 "print" "test") (error (list (quote wrong-type-arg-to-builtin) print 1 str-t)))
(assert-eq (print "print" 2 "test") (error (list (quote wrong-type-arg-to-builtin) print 2 str-t)))
(assert-eq (print "print" "test" 3) (error (list (quote wrong-type-arg-to-builtin) print 3 str-t)))

;; Error as argument.
(assert-eq (print (error "should not return")) (error (list (quote wrong-type-arg-to-builtin) print (error "should not return") str-t)))
