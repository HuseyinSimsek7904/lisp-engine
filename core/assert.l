(setg assert-eq (macro (-a -b)
                       (let ((a (eval -a)) (b (eval -b)))
                         (if (!= a b)
                             (error (list "expected equal" -a a -b b))))))

(setg assert-type (macro (-x -t)
                         (let ((x (eval -x)) (t (eval -t)))
                           (if (!= (typeof x) t)
                               (error (list "type mismatch" t -x x))))))

nil
