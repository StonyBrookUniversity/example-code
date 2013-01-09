#use "scheme.ml"
#use "testUtils.ml"

let test_eval x = List.map (fun y -> 
	print_endline ("% " ^ (unparse y));
	print_endline (string_of_value (eval y))) x
;;

let s_exprs = [
	(List [(Id "cons");(Num 1);(Id "nil")]) ;
	(List [(Id "cons");(Num 2);
		(List [(Id "cons");(Num 3);(Id "nil")])]) ;
	(List [(Id "car");
		(List [(Id "cons");(Num 4);
		(List [(Id "cons");(Num 5);(Id "nil")])])]) ;
	(List [(Id "cdr");
		(List [(Id "cons");(Num 6);
		(List [(Id "cons");(Num 7);(Id "nil")])])]) ;
	(List [(Id "if");(Bool true);(Num 1);(Num 2)]) ;
	(List [(Id "if");(Bool false);(Num 1);(Num 2)]) ;
] ;; 
 
test_eval s_exprs ;;
