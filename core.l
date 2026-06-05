(set not (fn (a) (if a nil t)))
(set and (fn (a b) (if a b nil)))
(set or (fn (a b) (if a t b)))

(set <= (fn (a b) (not (< b a))))
(set > (fn (a b) (< b a)))
(set >= (fn (a b) (not (< a b))))

(set != (fn (a b) (not (= a b))))
