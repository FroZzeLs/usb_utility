# Настройка debezium
## Добавление таблиц
Для добавления таблицы необходимо создать таблицу с идентичной структурой в **sink db**. Репликация выполняется пользователем **postgre_developer**. Ему необходимо выдать все права на обе БД, которые понадобятся для репликации:

```sql
-- Дать право подключения к базе
GRANT CONNECT ON DATABASE db_name TO postgre_developer;

-- Дать право создания объектов в базе (например, публикаций)
GRANT CREATE ON DATABASE db_name TO postgre_developer;

-- Выдать роль с правом репликации (требует суперпользователя)
ALTER ROLE postgre_developer WITH REPLICATION;

-- Дать права на использование схемы public
GRANT USAGE ON SCHEMA public TO postgre_developer;

-- Дать права SELECT на все таблицы в схеме public
GRANT SELECT ON ALL TABLES IN SCHEMA public TO postgre_developer;

-- Автоматически выдавать права SELECT на новые таблицы в схеме public
ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT SELECT ON TABLES TO postgre_developer;
```

**Если настраивается новая БД**, то для корректной работы необходимо создать 2 вспомогательных таблицы: 1 для **heartbeat**, 2 для управления через **incremental snapshot**. 

```sql
-- Создание heartbeat таблицы
CREATE TABLE IF NOT EXISTS public.debezium_heartbeat
(
    id integer NOT NULL,
    last_heartbeat timestamp without time zone NOT NULL DEFAULT now(),
    CONSTRAINT debezium_heartbeat_pkey PRIMARY KEY (id)
)
TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.debezium_heartbeat
    OWNER to postgres;

INSERT INTO public.debezium_heartbeat (id, last_heartbeat)
VALUES (1, NOW())
ON CONFLICT (id) DO NOTHING;

REVOKE ALL ON TABLE public.debezium_heartbeat FROM debezium;
REVOKE ALL ON TABLE public.debezium_heartbeat FROM postgre_developer;
REVOKE ALL ON TABLE public.debezium_heartbeat FROM repl_user;

GRANT SELECT, UPDATE ON TABLE public.debezium_heartbeat TO debezium;

GRANT SELECT ON TABLE public.debezium_heartbeat TO postgre_developer;

GRANT ALL ON TABLE public.debezium_heartbeat TO postgres;

GRANT SELECT ON TABLE public.debezium_heartbeat TO repl_user;
```
```sql
-- Создание сигнальной таблицы
CREATE TABLE IF NOT EXISTS public.dbz_signal
(
    id character varying(64) COLLATE pg_catalog."default" NOT NULL,
    type character varying(32) COLLATE pg_catalog."default",
    data character varying(2848) COLLATE pg_catalog."default",
    CONSTRAINT dbz_signal_pkey PRIMARY KEY (id)
)

TABLESPACE pg_default;

ALTER TABLE IF EXISTS public.dbz_signal
    OWNER to postgres;

REVOKE ALL ON TABLE public.dbz_signal FROM postgre_developer;
REVOKE ALL ON TABLE public.dbz_signal FROM repl_user;

GRANT SELECT ON TABLE public.dbz_signal TO postgre_developer;

GRANT ALL ON TABLE public.dbz_signal TO postgres;

GRANT SELECT ON TABLE public.dbz_signal TO repl_user;
```
Дальнейшее управление осуществляется через **kafka-connect-manager** (172.20.1.202). Для добавления новой таблицы нужно перейти в файл ***/home/administrator/kafka-connectors-manager/configs/config.yaml***. В данном файле необходимо описать 2 коннектора: 1 для **source db**, второй для **sink db**. 

Описание **source** коннектора выглядит следующим образом: 
```yaml
<source_db_name_table_name>:
  name: <source_db_name_table_name>
  source:
    db_name: <source_db_name>
    table_name: <table_name>
    stage: <dev/test/prom>
```
Описание **sink** коннектора имеет следующий вид:
```yaml
<sink_db_name_table_name>:
  name: <sink_db_name_table_name>
  sink:
    db_name: <sink_db_name>
    table_name: <table_name>
    stage: <dev/test/prom>
    pk_fields_name: <pk_field (id, date, etc)>
    topic_name: <source_db_name_table_name>
```
**Дополнительно уточняю, что в имени топика надо указать именно название source коннектора, т.к. именно на его топик подписывается sink коннектор.**

После добавления нужных коннекторов в конфиг сохраняете файл, в течение минуты **kafka-connect-manager** подтянет изменения, создаст коннекторы, нужные топики.

## Управление snapshot
### Изначально snapshot отключены, поэтому после создания/обновления коннектора snapshot не произойдёт.

Для запуска *snapshot* необходимо использовать сигнальный топик в **kafka-ui**. Топик называется по следующему шаблону:
***<db_name>.kafka.signal***
Для запуска *snapshot* нужно нажать кнопку **Produce Message**, в качестве **key** указать имя БД в которой выполняется *snapshot*, в качестве **value** следующее сообщение (в поле *data* перечисляются все таблицы, в которых надо запустить *snapshot*):
```json
{
	"type": "execute-snapshot",
	"data": {
		"data-collections": [
			"public.t1", 
            "public.t2",
            "custom.t5"
		]
	}
}
```
После публикации данного сообщения запустится *snapshot* на все указанные таблицы. Если данные не начали реплицироваться, то отследить прошёл ли сигнал на выполнение *snapshot* можно в топике ***<db_name>_dbz_signal*** либо в целевой БД в таблице ***dbz_signal***. Если *snapshot* не отобразился, проверьте правильность публикации сигнального сообщения, если все правильно, то смотрите логи *kafka-connect* и *PostgreSQL*.

## Если репликация перестала работать

**1**. Проверить статус *source* и *sink* коннекторов через запрос 
***http://172.20.1.219:8083/connectors/<connector_name>/status>***.
Если коннектор работает исправно, то вывод будет следующим:
```json
{
    "name": "connector_name",
    "connector": {
        "state": "RUNNING",
        "worker_id": "172.20.1.219:8083"
    },
    "tasks": [
        {
            "id": 0,
            "state": "RUNNING",
            "worker_id": "172.20.1.219:8083"
        }
    ],
    "type": "source/sink"
}
```
Если вывод отличается от представленного, значит с коннектором что-то не так. В таком случае ошибка либо будет выведена в ответе на запрос, либо на коннектор не будут назначены таски.
Логи **debezium** искать в логах **kafka-connect** (172.20.1.219).

**2**. Проверить слот репликации. выполните команду:
```sql
SELECT 
    slot_name, active,
    pg_size_pretty(pg_wal_lsn_diff(pg_current_wal_lsn(), confirmed_flush_lsn)) as actual_lag
FROM pg_replication_slots;
```
Если слот репликации имеет статус ***active = false***, то удалите слот репликации
```sql
SELECT pg_drop_replication_slot('<slot_name>')
```
После чего коннектор пересоздаст его и продолжит писать нормально.

**3**. Если первые шаги не помогли, попробуйте пересоздать коннектор. Для этого отправьте **DELETE** запрос на 
***http://172.20.1.219:8083/connectors/<connector_name>***

**4**. Если **3** шаг не принёс успеха анализируйе логи *kafka connect* и ищите причину самостоятельно.
