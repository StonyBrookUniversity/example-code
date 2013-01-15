#use "scheme.ml"
#use "testUtils.ml"

let test_eval x = List.map (fun y -> 
	print_endline ("% " ^ (unparse y));
	print_endline (string_of_value (eval y))) x
;;

let s_exprs = [
	(Num 42) ;
	(Bool true) ;
	(Id "nil") ; 
	(List [(Id "+");(Num 1);(Num 2)]) ;
	(List [(Id "+");(Num 1);(Num 2);(Num 3);(Num 4);(Num 5)]) ;
	(List [(Id "-");(Num 3)]) ;
	(List [(Id "-");(Num 3);(Num 4)]) ;
	(List [(Id "-");(Num 3);(Num 4);(Num 5)]) ;
	(List [(Id "*");(Num 1);(Num 2);(Num 3);(Num 4)]) ;
	(List [(Id "=");(Num 1);(Num 2)]) ;
	(List [(Id "=");(Num 1);(Num 1)]) ;
] ;; 
 
test_eval s_exprs ;;
