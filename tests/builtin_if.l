(setg assert-eq (fn (a b) (if (= a b) nil (error (list "expected equal" a b)))))

(assert-eq (if) (error (list (quote too-few-args-to-builtin) if 0 2)))
(assert-eq (if 1) (error (list (quote too-few-args-to-builtin) if 1 2)))

(assert-eq (if 1 2) (error (list (quote wrong-type-arg-to-builtin) if 1 bool-t)))

(assert-eq (if (error "should return") "anything") (error "should return"))

(assert-eq (if false "anything") nil)
(assert-eq (if false anything) nil)

(assert-eq (if true "anything" (error "something")) "anything")
(assert-eq (if true anything (error "something")) (error (list (quote undefined-symbol) (quote anything))))

(assert-eq (if false "hello" "hi" "how are you") "how are you")
(assert-eq (if false "hello" (error "should return") "how are you") (error "should return"))
