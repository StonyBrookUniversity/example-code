#use "scheme.ml";;
#use "testUtils.ml";;

let rec unparse_list2 = function
    [] -> ""
  | (x::[]) -> unparse2 x
  | (x::xs) -> (unparse2 x) ^ ";" ^ (unparse_list2 xs)

and unparse2 = function
  | Id id -> "(Id \"" ^ id ^ "\")"
  | Num n -> "(Num " ^ string_of_int n ^ ")"
  | Bool true -> "(Bool true)"
  | Bool false -> "(Bool false)"
  | String s -> "(String \"" ^ s ^ "\")"
  | List l -> "(List [" ^ unparse_list2 l ^ "])"
;;

let test_parse x = List.map (fun y -> 
	print_endline ("% " ^ y) ;
	print_endline (unparse2 (parse (tokenize y))))
	x
;;
let test_eval x = List.map (fun y -> 
	print_endline ("% " ^ (unparse y));
	print_endline (string_of_value (eval y))) x
;;

(* Your test cases here *)

test_parse [ "5" ] ;;

test_eval [ (Num 5) ] ;;

