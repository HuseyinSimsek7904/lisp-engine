(include "core/assert.l")

;; Function application should use the environment that was used to define the function.
(let ((x (atom 3)))
  (set f1 (fn (y) (+ (deref x) y)))

  (assert-eq (f1 5) 8)
  (assert-eq (f1 15) 18)

  (let ((x 6))
    (assert-eq (f1 5) 8)
    (assert-eq (f1 15) 18))

  (reset x 5)
  (assert-eq (f1 5) 10)
  (assert-eq (f1 15) 20))

(set f2 (fn (y) x))

(assert-eq (f2 5)
           (error '(undefined-symbol x)))
(assert-eq (let ((x 10)) (f2 5))
           (error '(undefined-symbol x)))

;; Try partial application.
(set add-curry (fn (x) (fn (y) (+ x y))))

(assert-type add-curry fn-t)
(assert-type (add-curry 1) fn-t)
(assert-eq ((add-curry 1) 2) 3)
