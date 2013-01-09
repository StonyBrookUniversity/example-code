
(* --------------------------------- *)
(* int *)

let prt_int x = print_endline (string_of_int x)
;;

(* --------------------------------- *)
(* int list *)

let string_of_int_list l =
    let rec string_of_int_elements l = match l with
          [] -> ""
        | (h::[]) -> string_of_int h
        | (h::t) -> string_of_int h ^ ";" ^ string_of_int_elements t
    in "[" ^ string_of_int_elements l ^ "]"
;;

let prt_int_list l = print_endline (string_of_int_list l)
;;

(* --------------------------------- *)
(* int list list *)

let rec string_of_int_list_list l = match l with
      [] -> ""
    | (h::t) -> (string_of_int_list h) ^ (string_of_int_list_list t)
;;

let prt_int_list_list l = print_endline (string_of_int_list_list l)
;;

(* --------------------------------- *)
(* bool *)

let prt_bool x = print_endline (string_of_bool x)
;;

(* --------------------------------- *)
(* bool list *)

let string_of_bool_list l =
    let rec string_of_bool_elements l = match l with
          [] -> ""
        | (h::[]) -> string_of_bool h
        | (h::t) -> string_of_bool h ^ ";" ^ string_of_bool_elements t
    in "[" ^ string_of_bool_elements l ^ "]"
;;

let prt_bool_list l = print_endline (string_of_bool_list l)
;;

(* --------------------------------- *)
(* int tuple *)

let string_of_int_tuple_list (a,b) =
  "(" ^ string_of_int_list a ^ ", " ^ string_of_int_list b ^ ")"
;;

let prt_int_tuple_list (a,b) = print_endline (string_of_int_tuple_list (a,b))
;;

(* --------------------------------- *)
(* char *)

let rec string_of_char_list = function
      [] -> ""
    | (h::t) -> (Char.escaped h) ^ " " ^ (string_of_char_list t)
;;

let prt_char_list x = print_endline (string_of_char_list x)
;;

let prt_char_list_sorted x = print_endline 
	(string_of_char_list (List.sort Char.compare x))
;;

(* --------------------------------- *)
(* string list *)

let string_of_str_list l =
    let rec string_of_str_elements l = match l with
          [] -> ""
        | (h::[]) -> h
        | (h::t) -> h ^ ";" ^ string_of_str_elements t
    in "[" ^ string_of_str_elements l ^ "]"
;;

let prt_str_list l = print_endline (string_of_str_list l)
;;


