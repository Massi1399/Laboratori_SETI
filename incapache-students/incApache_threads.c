/*
 * incApache_threads.c: implementazione dei thread per il web server del corso di SET
 *
 * Programma sviluppato a supporto del laboratorio di
 * Sistemi di Elaborazione e Trasmissione del corso di laurea
 * in Informatica classe L-31 presso l'Universita` degli Studi di
 * Genova, per l'anno accademico 2022/2023.
 *
 * Copyright (C) 2012-2014 by Giovanni Chiola <chiolag@acm.org>
 * Copyright (C) 2015-2016 by Giovanni Lagorio <giovanni.lagorio@unige.it>
 * Copyright (C) 2016-2022 by Giovanni Chiola <chiolag@acm.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "incApache.h"

pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mime_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef INCaPACHE_6_1
int client_sockets[MAX_CONNECTIONS];			/* for each connection, its socket FD */
int no_response_threads[MAX_CONNECTIONS]; /* for each connection, how many response threads */

pthread_t thread_ids[MAX_THREADS];
int connection_no[MAX_THREADS];	 /* connection_no[i] >= 0 means that i-th thread belongs to connection connection_no[i] */
pthread_t *to_join[MAX_THREADS]; /* for each thread, the pointer to the previous (response) thread, if any */

int no_free_threads = MAX_THREADS - 2 * MAX_CONNECTIONS;						 /* each connection has one thread listening and one reserved for replies */
struct response_params thread_params[MAX_THREADS - MAX_CONNECTIONS]; /* params for the response threads (the first MAX_CONNECTIONS threads are waiting/parsing requests) */

pthread_mutex_t threads_mutex = PTHREAD_MUTEX_INITIALIZER; /* protects the access to thread-related data structures */

//utilizzata da find_unused_thread
static int reserve_unused_thread()
{
	int idx;
	for (idx = MAX_CONNECTIONS; idx < MAX_THREADS; ++idx)
		if (connection_no[idx] == FREE_SLOT)
		{
			connection_no[idx] = RESERVED_SLOT;
			return idx;
		}
	assert(0);
	return -1;
}

//cerca tra i threads disponibili uno da riservare
int find_unused_thread_idx(int conn_no)
{
	int idx = -1;
	pthread_mutex_lock(&threads_mutex);
	if (no_response_threads[conn_no] > 0)
	{ /* reserved thread already used, try to find another (unused) one */
		if (no_free_threads > 0)
		{
			--no_free_threads;
			++no_response_threads[conn_no];
			idx = reserve_unused_thread();
			pthread_mutex_unlock(&threads_mutex);
			return idx;
		}
		pthread_mutex_unlock(&threads_mutex);
		join_all_threads(conn_no);
		pthread_mutex_lock(&threads_mutex);
	}
	assert(no_response_threads[conn_no] == 0);
	no_response_threads[conn_no] = 1;
	idx = reserve_unused_thread();
	pthread_mutex_unlock(&threads_mutex);
	return idx;
}

//effettua la join dei threads della connessione passata come parametro
void join_all_threads(int conn_no)
{
	size_t i;

	/*** compute the index i of the thread to join,
	 *** call pthread_join() on thread_ids[i], and update shared variables
	 *** no_free_threads, no_response_threads[conn_no], and
	 *** connection_no[i] ***/
	/*** TO BE DONE 6.1 START ***/

//non servono mutex perché vogliamo che tutti i thread per questa connessione aggiornino le variabili condivise

	if (to_join[conn_no] != NULL)
	{
	//calcola l'indice del thread da joinare
		for (i = MAX_CONNECTIONS; i < MAX_THREADS && to_join[conn_no] != &thread_ids[i]; ++i);

//la join non ritorna errno in caso di errore 
//è più opportuno usare una variabile di tipo intptr_t e passarla al posto di NULL come (void**) per salvare il valore di ritorno

		if (pthread_join(thread_ids[i], NULL) != 0)
			fail_errno("In join_all_threads function, join() error");
	
	//se per quella connessione non ci sono più thread di risposta
		if (--no_response_threads[conn_no] == 0)
	//allora aumenta il counter di thread liberi
			++no_free_threads;
	//(forse sarebbe meglio togliere l'if; quindi decrementare il contenuto dell'array alla posizione conn_no
	// ed evitare di aumentare i thread_liberi solo se ci sono 0 thread di risposta per quella connessione )		
			
	//marca la posizione i dell'array delle connesioni come libero	
		connection_no[i] = FREE_SLOT;
	//imposta il puntatore al thread precedente come null
		to_join[conn_no] = NULL;
	}

	/*** TO BE DONE 6.1 END ***/
}

//viene richiamata in http.c dalla send_response per attendere il thread precedente (che abbia risposto)
void join_prev_thread(int thrd_no)
{
	size_t i;
	int conn_no;
	debug("start of join_prev_thread(%d)\n", thrd_no);

	/*** check whether there is a previous thread to join before
	 *** proceeding with the current thread.
	 *** In that case compute the index i of the thread to join,
	 *** wait for its termination, and update the shared variables
	 *** no_free_threads, no_response_threads[conn_no], and connection_no[i],
	 *** avoiding race conditions ***/
	/*** TO BE DONE 6.1 START ***/

//controlla se c'è un thread precedente da joinare
	if (to_join[thrd_no] != NULL)
	{
	//se c'è calcola l'indice	 
		for (i = MAX_CONNECTIONS; i < MAX_THREADS && to_join[thrd_no] != &thread_ids[i]; ++i);
		if (pthread_join(thread_ids[i], NULL) != 0)
			fail_errno("In join_prev_thread function, join() error");
		//if (i >= MAX_CONNECTIONS)
		//{
		//ottengo il numero di connessione a cui appartiene il thread
			conn_no = connection_no[thrd_no];
		//serve la mutua esclusione per far sì che solo per il thread joinato si aggiornino le variabili	
			if (pthread_mutex_lock(&threads_mutex) != 0)
				fail("Cannot mutex lock");
				
			if (--no_response_threads[conn_no] == 0)
				++no_free_threads;
			connection_no[i] = FREE_SLOT;
			
			if (pthread_mutex_unlock(&threads_mutex) != 0)
				fail("Cannot mutex unlock");
		//}
	}

	/*** TO BE DONE 6.1 END ***/
}

void *response_thread(void *vp)
{
	size_t thread_no = ((int *)vp) - connection_no;
	int connection_idx = *((int *)vp);
	debug(" ... response_thread() thread_no=%lu, conn_no=%d\n", (unsigned long)thread_no, connection_idx);
	const size_t i = thread_no - MAX_CONNECTIONS;
	send_response(client_sockets[connection_idx],
								thread_params[i].code,
								thread_params[i].cookie,
								thread_params[i].is_http1_0,
								(int)thread_no,
								thread_params[i].filename,
								thread_params[i].p_stat);
	debug(" ... response_thread() freeing filename and stat\n");
	free(thread_params[i].filename);
	free(thread_params[i].p_stat);
	return NULL;
}

#else /* #ifndef INCaPACHE_6_1 */

pthread_t thread_ids[MAX_CONNECTIONS];
int connection_no[MAX_CONNECTIONS];

#endif /* ifdef INCaPACHE_6_1 */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
void *client_connection_thread(void *vp)
{
	int client_fd;
	struct sockaddr_storage client_addr;
	socklen_t addr_size;
#ifdef INCaPACHE_6_1
	pthread_mutex_lock(&threads_mutex);
	int connection_no = *((int *)vp);

	/*** properly initialize the thread queue to_join ***/
	/*** TO BE DONE 6.1 START ***/
	//imposta il primo thread per quella connessione a null
	to_join[connection_no] = NULL;
	/*** TO BE DONE 6.1 END ***/

	pthread_mutex_unlock(&threads_mutex);
#endif
	for (;;)
	{
		addr_size = sizeof(client_addr);
		pthread_mutex_lock(&accept_mutex);
		if ((client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_size)) == -1)
			fail_errno("Cannot accept client connection");
		pthread_mutex_unlock(&accept_mutex);
#ifdef INCaPACHE_6_1
		client_sockets[connection_no] = client_fd;
#endif
		char str[INET_ADDRSTRLEN];
		struct sockaddr_in *ipv4 = (struct sockaddr_in *)&client_addr;
		printf("Accepted connection from %s\n", inet_ntop(AF_INET, &(ipv4->sin_addr), str, INET_ADDRSTRLEN));
		manage_http_requests(client_fd
#ifdef INCaPACHE_6_1
												 ,
												 connection_no
#endif
		);
	}
}
#pragma clang diagnostic pop

char *get_mime_type(char *filename)
{
	char *mime_t = NULL;
	const size_t len_filename = strlen(filename);
	ssize_t nchars_read = 0;
	size_t sz = 0;
	if (len_filename > 4 && strcmp(filename + len_filename - 4, ".css") == 0)
		return my_strdup("text/css;");
	debug("      ... get_mime_type(%s): was not .css\n", filename);

	/*** What is missing here to avoid race conditions ? ***/
	/*** TO BE DONE 6.0 START ***/

	if (pthread_mutex_lock(&mime_mutex) != 0)
		fail("In get_mime_type function, mutex_lock() error:");

	/*** TO BE DONE 6.0 END ***/

	fprintf(mime_request_stream, "%s\n", filename);
	fflush(mime_request_stream);
	debug("      ... get_mime_type(%s): printed filename on mime_request_stream\n", filename);
	if ((nchars_read = getline(&mime_t, &sz, mime_reply_stream)) < 1)
		fail("Could not get answer from file");

	/*** What is missing here to avoid race conditions ? ***/
	/*** TO BE DONE 6.0 START ***/

	if (pthread_mutex_unlock(&mime_mutex) != 0)
		fail("In get_mime_type function, mutex_unlock() error:");

	/*** TO BE DONE 6.0 END ***/

	if (mime_t[--nchars_read] == '\n')
		mime_t[nchars_read] = '\0';
	debug("      ... get_mime_type(%s): got answer %s\n", filename, mime_t);
	return mime_t;
}

#ifdef INCaPACHE_6_1

//viene chiamata nella manage_http_request
//new_thread_idx è il risultato della chiamata a find_unused_threads
void send_resp_thread(int out_socket, int response_code, int cookie,
											int is_http1_0, int connection_idx, int new_thread_idx,
											char *filename, struct stat *stat_p)
{
	struct response_params *params = thread_params + (new_thread_idx - MAX_CONNECTIONS);
	debug(" ... send_resp_thread(): idx=%lu\n", (unsigned long)(params - thread_params));
	params->code = response_code;
	params->cookie = cookie;
	params->is_http1_0 = is_http1_0;
	params->filename = filename ? my_strdup(filename) : NULL;
	params->p_stat = stat_p;
	pthread_mutex_lock(&threads_mutex);
	connection_no[new_thread_idx] = connection_idx;
	debug(" ... send_resp_thread(): parameters set, conn_no=%d\n", connection_idx);

	/*** enqueue the current thread in the "to_join" data structure ***/
	/*** TO BE DONE 6.1 START ***/

//se alla posizione connection_idx dell'array to_join non c'è nessun puntatore al thread precedente 	
	if (to_join[connection_idx] == NULL)
	{
	//assegno l'indirizzo del nuovo thread per quella connessione
		to_join[connection_idx] = &thread_ids[new_thread_idx];
	//e imposto a null il puntatore del primo thread per quella connessione	
		to_join[new_thread_idx] = NULL;
	}
	else
	//altrimenti   
	{
	//nella posizione del nuovo thread salvo il puntatore a quello precendente
		to_join[new_thread_idx] = to_join[connection_idx];
	//aggiorno l'indirizzo del nuovo thread	
		to_join[connection_idx] = &thread_ids[new_thread_idx];
	}
	/*** TO BE DONE 6.1 END ***/

	if (pthread_create(thread_ids + new_thread_idx, NULL, response_thread, connection_no + new_thread_idx))
		fail_errno("Could not create response thread");
	pthread_mutex_unlock(&threads_mutex);
	debug(" ... send_resp_thread(): new thread created\n");
}

#endif
