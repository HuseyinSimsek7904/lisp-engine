(set merge2 (fn (x y) (list (unpack x) (unpack y))))
(set merge (fn ((rest xs))
               (if (nil? xs) nil
                   (merge2 (head xs) (merge (unpack (tail xs)))))))
