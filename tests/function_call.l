(include "core/assert.l")

;; Function application should use the environment that was used to define the function.
(let ((x 3)) (setg f1 (fn (y) (+ x y))))

(assert-eq (f1 5) 8)
(assert-eq (f1 15) 18)

(let ((x 6))
  (assert-eq (f1 5) 8)
  (assert-eq (f1 15) 18))

(setg f2 (fn (y) (+ x y)))

(assert-eq (f2 5)
           (error (list 'wrong-type-arg-to-builtin +
                        (error (list 'undefined-symbol 'x)) int64-t)))
(assert-eq (let ((x 10)) (f2 5))
           (error (list 'wrong-type-arg-to-builtin +
                        (error (list 'undefined-symbol 'x)) int64-t)))

;; The environment variables can be updated after the function definition.
;; The function should use the updated variables.
(let ((x 3))
  (setg f3 (fn (y) (+ x y)))
  (set x 5))

(assert-eq (f3 5) 10)

;; Try partial application.
(setg add-curry (fn (x) (fn (y) (+ x y))))

(assert-type add-curry fn-t)
(assert-type (add-curry 1) fn-t)
(assert-eq ((add-curry 1) 2) 3)
