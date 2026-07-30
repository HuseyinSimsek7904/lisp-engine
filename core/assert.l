(set assert-eq (fn (a b) (if (!= a b)
                             (error (list 'assert-failed (list '= a b))))))

(set assert-type (fn (x t) (if (!= t (typeof x))
                               (error (list 'assert-failed (list '= t (typeof x)))))))
