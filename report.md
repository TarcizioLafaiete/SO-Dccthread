# PAGINADOR DE MEMÓRIA - RELATÓRIO

1. Termo de compromisso

Os membros do grupo afirmam que todo o código desenvolvido para este
trabalho é de autoria própria.  Exceto pelo material listado no item
3 deste relatório, os membros do grupo afirmam não ter copiado
material da Internet nem ter obtido código de terceiros.

2. Membros do grupo e alocação de esforço

Preencha as linhas abaixo com o nome e o e-mail dos integrantes do
grupo.  Substitua marcadores `XX` pela contribuição de cada membro
do grupo no desenvolvimento do trabalho (os valores devem somar
100%).

  * Gabriel Bifano Freddi <email@domain> 25%
  * Pedro de Oliveira Guedes <email@domain> 30%
  * Tarcizio Augusto Santos Lafaiete <tarcizio-augusto@hotmail.com> 45%

3. Referências bibliográficas
  https://pubs.opengroup.org/onlinepubs/7908799/xsh/ucontext.h.html
  https://www.codeproject.com/Tips/4225/Unix-ucontext-t-Operations-on-Windows-Platforms
  https://man7.org/linux/man-pages/man2/timer_create.2.html
  https://manpages.ubuntu.com/manpages/trusty/en/man2/timer_create.2.html


4. Estruturas de dados

  1. Descreva e justifique as estruturas de dados utilizadas para
     gerência das threads de espaço do usuário (partes 1, 2 e 5).

      Para o gerênciamento de threads foi criado uma estrutra chamamda managerThreads, ele é basicamente um container no qual estão as filas de threads, o ucontext_t da thread gerente e a dccthread_t atual em execução.

      Explicando cada uma de maneira, mais especfíca, o manager tem em seu contexto a função managerCentral, ali é feito as trocas de contexto realizadas entres a threads, seu funcionamento é simples, apenas capturando da 
    fila de prontos a proxima thread e trocando o contexto para esta, toda vez que thread acaba ela retorna para o contexto da thread gerente. Nós possuimos 3 filas de threads, sendo elas a fila de prontos, a fila de espera e 
    a fila de threads em hibernação, todas estas utilizam a struct dlist fornecida juntamente com o TP e funcionam como uma FIFO no programa sempre liberando a mais antiga thread disponível para o current_thread que é o marcador 
    de qual a thread que está com seu contexto em execução no momento, o que é importante para a gerência das funções self, name, exit e wait. Aproveitando para falar do dccthread_t,  nesta struct temos o ucontext_t responsavel por
    guardar o contexto, o nome que é pré-requisito do TP, uma variavel de blocking que auxilia nas condições de corrida e sera melhor explicada no tópico 2, um id que é unico para cada thread e um idWaited que representa se a há 
    alguma outra thread esperando aquela thread. Na dccthread_wait realizamos uma pesquisa nas filas de prontos e hibernação para verificar se a thread que deve ser esperada realmente existe em seguida gravamos o id da thread atual
    no idWaited desta thread, colocamos a current_thread na fila de espera e a thread tid na fila de prontos, no momento de realizarmos o exit, verificamos o idWaited para verificar se há alguma thread a espera, caso tenha, retiramos 
    a thread da fila de esperamos e retornamos ela para a fila de prontos.

      Na parte 5 do TP, colocamos a current_thread na fila de hibernação, criamos um timer com as funções especificadas no TP e suas structs corelacionadas que chama uma função de tratamento que acorda a thread apos o tempo pré-determinado 
    pelo usuário e chamamos a thread gerente para colocar a próxima thread da fila para executar, e assim a gerência fica semelhante aos outras partes do TP, até que o timer finalmente é esgotado e chama a função __sleep, nesta função, 
    passamos a thread da fila de hibernação para o fila de prontos, deletamos o timer já que ele não é mais útil e chamamos o gerente.   

  2. Descreva o mecanismo utilizado para sincronizar chamadas de
     dccthread_yield e disparos do temporizador (parte 4).

      Na parte 4, para evitar as condições de corrida e sincronizar o dcctherad_yield com os disparos do temporizador, utilizamos das funções block() e unblock(), estas funções desbilitam e habilitam o timer de preempção respectivamente, 
    assim sendo, sempre que estamos em um yield ou em outras funções não corre o risco do timer ser disparado e causas um funcionamento indesejado. Para as funções de block e unblock nos não utilizamos o sigprocmask como recomendado no TP
    devido a problemas encontrados pelo grupo ao implementá-lo, com isso adotamos a política de deletar o timer no momento do block e reconstrui-lo no momento do unblock, aqui utilizamos o blocking do dcctheread_t para verificar se já houve
    um block e não realizar um segundo antes de um unblock. Obviamente, esta implementação não é ótima, pois como resetamos o timer ele provavelmente durará mais dos 10ms.
