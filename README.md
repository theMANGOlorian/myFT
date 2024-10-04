Il progetto MyFT è un’applicazione client/server progettata per il trasferimento
file.
L’applicazione consente ai client di caricare, scaricare e visualizzare i file
presenti sul server. Il server è configurabile tramite parametri passati da riga di
comando.
Il progetto comprende un client e un server.
Il server MyFT è progettato per gestire il trasferimento di file tra client e server.
L'applicazione utilizza la programmazione multi-processing per consentire a
più client di connettersi e trasferire file simultaneamente.
La configurazione del server avviene tramite parametri passati da riga di
comando, che includono l'indirizzo IP, la porta di ascolto e la directory radice
dove verranno memorizzati i file. La directory radice se non esiste verrà creata
dal programma.
Il client MyFT è progettato per inviare delle richiesta di operazioni per il server.
La configurazione del client avviene tramite parametri passati da riga di
comando, che includono operazione richiesta ,l'indirizzo IP, la porta di ascolto
e i percorsi dei file.
Per garantire l'integrità dei dati durante il trasferimento, il server implementa
un sistema di lock sia in lettura che in scrittura sui file, prevenendo accessi
simultanei che potrebbero causare corruzione dei dati. Inoltre, il server verifica
lo spazio disponibile sul file system prima di accettare un file in upload,
assicurando che ci sia sufficiente spazio per completare l'operazione.
La comunicazione avviene nel seguente modo:
Il client invierà al server una richiesta (PUT,GET o INF) in base alle sue
esigenze, se dovrà eseguire una operazione di download manderà una
richiesta GET, se dovrà eseguire una operazione di upload manderà una
richiesta PUT o se dovrà visualizzare i file presenti nel server invierà una
richiesta INF.
Quando il server riceve ed elabora la richiesta invierà un riscontro, se positivo
procede nell'effettuare l’operazione richiesta dal client, se negativo interrompe
la comunicazione.
in caso di richiesta PUT (scrittura), il server controlla se lo spazio nel sistema
è sufficiente per ospitare il file.
In caso di richiesta INF, il server inizia creando una pipe per consentire la
comunicazione tra il processo padre e il processo figlio. Successivamente,
chiama fork per creare un nuovo processo. Nel processo figlio chiude il lato di
lettura della pipe, reindirizza stdout alla pipe e esegue il comando ls -la. Nel
processo padre, chiude il lato di scrittura della pipe e legge l'output del
comando ls -la dalla pipe usando. Continua a leggere finché ci sono dati da
leggere.
