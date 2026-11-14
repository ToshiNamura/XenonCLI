; examples/hello.asm
; Собрать:  xenon asm examples/hello.asm -o out/hello.module
; Запустить: xenon run out/hello.module

.meta
    .module Hello
.end

.text
    .function export double main()
        const 42.0
        ret
    .end
.end
