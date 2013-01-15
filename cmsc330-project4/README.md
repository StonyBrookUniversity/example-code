cmsc330-project4
============

<em>CMSC330 Project 4: Scheme Parser & Interpreter (http://www.cs.umd.edu/class/spring2010/cmsc330/p4/)</em>

Description / Overview
---
CMSC330 (Organization of Programming Languages) explores the features of programming languages and what sets them apart. In this class we were asked to write programs in Ruby, OCaml, Scheme, and Java. Project 4 consisted of two of those programming languages - we were asked to write a (basic) Scheme interpreter in OCaml.

My implementation for this project is in [/scheme.ml](https://github.com/ericnorris/example-code/blob/master/cmsc330-project4/scheme.ml). To launch an interactive Scheme interpreter, run:

    ocaml main.ml

To use the public tests provided by the instructor at the time of the project, run:

    ocaml test/public_eval1.ml
    ocaml test/<test_file_here>.ml

To compare this output with the correct output of the test (as determined by the instructor), run:

    ocaml test/public_eval1.ml | diff output/public_eval1.out -
    ocaml test/<test_file_here>.ml | diff output/<test_output_here>.ml -

Thoughts / Experience
---
Working with OCaml was an entirely new experience for me; having never encountered a functional programming language before, it took a lot to get out of the procedural programming mindset. It was refreshing (albeit occassionally difficult) to take on the challenge of thinking in a functional way.

As a personal test, the version here on GitHub is a rewrite of the version I submitted to the class, but without any warnings from the OCaml interpreter. It was fun to look at my old code and improve it with the skills and knowledge I have now.
