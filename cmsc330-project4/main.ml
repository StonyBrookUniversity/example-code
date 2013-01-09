(* Here is a sample main.ml file that uses your lexer, parser, and
   evaluator to read in lines of Scheme and evaluate them.  It reads
   until it sees ;;, concatenating all the lines together, and then
   feeds that to your code. As a bonus, it discards any comments
   beginning with ; *)

#use "scheme.ml"

let buffer = ref "";;

try
  while true do
    let _ = print_string "Scheme> " in
    let line = read_line () in
    try
      let i = String.index line ';' in
      let _ = buffer := (!buffer) ^ (String.sub line 0 i) in
      if String.length line >= (i+2) && line.[(i+1)] = ';' then
	let tokens = tokenize (!buffer) in
	let tree = parse tokens in
	let v = eval tree in
	begin
	  Printf.printf "%s evaluates to %s\n" (unparse tree) (string_of_value v);
	  buffer := ""
	end
   with Not_found -> buffer := (!buffer) ^ line
  done
with End_of_file -> ()
;;
