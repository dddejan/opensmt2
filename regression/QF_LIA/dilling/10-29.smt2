(set-info :smt-lib-version 2.6)
(set-logic QF_LIA)
(set-info :category "industrial")
(set-info :status sat)
(declare-fun x0 () Int)
(declare-fun x1 () Int)
(declare-fun x2 () Int)
(declare-fun x3 () Int)
(declare-fun x4 () Int)
(declare-fun x5 () Int)
(declare-fun x6 () Int)
(declare-fun x7 () Int)
(declare-fun x8 () Int)
(declare-fun x9 () Int)
(assert (let ((?v_7 (* 0 x0)) (?v_17 (* 1 x2)) (?v_3 (* 0 x6)) (?v_5 (* 0 x8)) (?v_4 (* 1 x0)) (?v_1 (* 0 x1)) (?v_2 (* 0 x4)) (?v_6 (* 1 x5)) (?v_9 (* 0 x2))) (let ((?v_19 (+ ?v_3 (* 2 x8))) (?v_8 (* 1 x7)) (?v_10 (* 0 x9)) (?v_12 (* 1 x3)) (?v_11 (* 1 x1)) (?v_18 (* 0 x5)) (?v_0 (* (- 1) x9)) (?v_14 (* (- 1) x3)) (?v_15 (* (- 1) x0)) (?v_16 (* (- 1) x7)) (?v_13 (* (- 1) x6))) (and (<= (+ ?v_7 (+ ?v_17 (+ (* (- 2) x3) (+ ?v_0 (+ ?v_3 (+ ?v_5 (* 1 x4))))))) 0) (<= (+ ?v_4 (+ ?v_1 (+ ?v_14 (+ ?v_0 (+ ?v_2 ?v_6))))) 1) (<= (+ ?v_15 (+ ?v_0 (+ ?v_1 (+ ?v_9 (+ ?v_2 ?v_19))))) 0) (<= (+ ?v_4 (+ ?v_1 (+ (* 0 x3) (+ ?v_2 (+ ?v_3 ?v_8))))) 1) (<= (+ ?v_16 (+ ?v_5 (+ ?v_10 (+ ?v_12 (+ ?v_6 (* 1 x6)))))) 0) (<= (+ ?v_7 (+ ?v_13 (+ (* (- 1) x8) (+ ?v_11 (+ ?v_2 (+ ?v_8 (* 2 x9))))))) 0) (<= (+ ?v_9 (+ ?v_10 (+ ?v_11 (+ ?v_12 (+ ?v_6 (+ ?v_8 (* 1 x8))))))) 1) (<= (+ (* (- 1) x5) (+ (* (- 2) x9) (+ ?v_13 (+ ?v_9 (+ ?v_11 ?v_14))))) (- 1)) (<= (+ ?v_15 (+ ?v_16 (+ ?v_17 (+ ?v_2 (+ ?v_18 (+ ?v_5 ?v_10)))))) 0) (<= (+ ?v_17 (+ (* (- 2) x4) (+ ?v_18 (+ ?v_0 ?v_19)))) (- 1))))))
(check-sat)
(exit)
