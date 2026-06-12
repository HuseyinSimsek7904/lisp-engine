
(setg not (fn (a) (if a nil t)))
(setg and (fn (a b) (if a b nil)))
(setg or (fn (a b) (if a t b)))

(setg <= (fn (a b) (not (< b a))))
(setg > (fn (a b) (< b a)))
(setg >= (fn (a b) (not (< a b))))

(setg != (fn (a b) (not (= a b))))
