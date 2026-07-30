(set assert (macro (c) `(if (true? ~c) nil (error `(assert-failed ~c)))))

(set assert-eq (macro (a b) `(assert (= ~a ~b))))

(set assert-type (macro (x t) `(assert (= ~t (typeof ~x)))))
