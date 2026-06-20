(setg include (fn (file-name) (eval (unpack (parse-lisp (read-file file-name))))))

(setg not (fn (a) (if a false true)))
(setg and (fn (a b) (if a b false)))
(setg or (fn (a b) (if a true b)))

(setg <= (fn (a b) (not (< b a))))
(setg > (fn (a b) (< b a)))
(setg >= (fn (a b) (not (< a b))))

(setg != (fn (a b) (not (= a b))))
