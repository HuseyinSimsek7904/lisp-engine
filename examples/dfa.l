;; These macros allows for defining Deterministic Finite Automatas using tables of transitions,
;; accepting and initial states. Using the macros, it is possible to generate a function that
;; decides whether a string is an element of the language defined by the DFA.

;; Example DFA that decides whether a string ends with ('a 'b 'b):

;; Accepting states: (s3)
;; Initial state: s0
;; State transitions:
;; (s0 (a s1) (b s0))
;; (s1 (a s1) (b s2))
;; (s2 (a s1) (b s3))
;; (s3 (a s1) (b s0))

;; Transition function is a function step: Sym * State -> State
;; step(i, s) returns the next state, given the current symbol is i and
;; the current state is s.

;; Accepting function is a function accept: State -> Bool
;; accept(s) returns whether or not the state s is an accepting state.

(set dfa:gen-step (macro (trans)
                         (set _case-input (fn (cs)
                                              (if (empty? cs) nil
                                                  `(if (= input '~(head (head cs)))
                                                       '~(head (head cs) 1)
                                                       ~(_case-input (tail cs))))))

                         (set _case-state (fn (trans)
                                              (if (empty? trans) nil
                                                  `(if (= state '~(head (head trans)))
                                                       ~(_case-input (tail (head trans)))
                                                       ~(_case-state (tail trans))))))

                         (print (format "Transformation table: %l" trans))

                         `(fn (input state) ~(_case-state trans))))

(set dfa:gen-accept (macro (accept)
                           (set _contains? (fn (accept)
                                               (if (empty? accept) 'false
                                                   `(if (= state '~(head accept)) true
                                                        ~(_contains? (tail accept))))))

                           (print (format "Accept states: %l" accept))

                           `(fn (state) ~(_contains? accept))))

(set dfa (fn (step accept init)
             (set _decide (fn (input state)
                              (if (empty? input)
                                  (accept state)
                                  (set next (step (head input) state))
                                  (print (format "%l %l -> %l" (head input) state next))
                                  (_decide (tail input) next))))

             (fn (input) (_decide input init))))

;; Testing with example DFA setup.
(include "core/assert.l")

(set pr (fn (f x) (print (format f x)) x))

(set dfa1 (let ((step (dfa:gen-step ((s0 (a s1) (b s0))
                                     (s1 (a s1) (b s2))
                                     (s2 (a s1) (b s3))
                                     (s3 (a s1) (b s0)))))

                (accept (dfa:gen-accept (s3)))

                (init 's0))

            (dfa step accept init)))

(let ((test1 (fn (i)
                 (set result (dfa1 i))
                 (print (format "%l >>> %l" i result))
                 result)))

  (assert-eq (test1 '(a)) false)
  (assert-eq (test1 '(b)) false)
  (assert-eq (test1 '(a a)) false)
  (assert-eq (test1 '(a b)) false)
  (assert-eq (test1 '(a a b)) false)
  (assert-eq (test1 '(a b b a)) false)

  (assert-eq (test1 '(a b b)) true)
  (assert-eq (test1 '(a a b b)) true)
  (assert-eq (test1 '(b a b b)) true)
  (assert-eq (test1 '(a b a b a b b)) true)
  )
