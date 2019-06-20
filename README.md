# Capy RabbitMQ wrap library for C++

Acинхронный клиент к брокеру сообщений реализующий шаблон PFL - Publish/Fetch/Listen. 
Предполагается, что поддержка асинхронности создания самого объекта (соединения) реализуется средствами конечного приложения. 
Т.е. нельзя реюзать объекты брокера в разных потоках, но сам брокер исполняет замыкания в пуле потоков. 
Размер пула потоков задается автоматически в зависимости от количества физических ядер хоста.  
Доставка и получение собщений к брокеру остается асинхронной и не блокирует клиентское приложение.

***TODO***: настройка параметров соединения (таймауты и тп...., кстомный размер пула асинхронных вызывов)

## Зависимости
1. https://github.com/alanxz/rabbitmq-c - низкоуровневый API к RabbitMQ
1. https://github.com/aithea/capy-dispatchq - обертка к thread-pool
1. https://github.com/google/googletest 
1. https://github.com/nlohmann/json - быстрый высокоуровневый сахар для работы 
со сложными структурами данных в стиле json-объектов, поддерживает бинарную 
сериализацию данных и двухсторонний маппинг в/из json 
1. git/git-flow
1. cmake>=3.12
1. clang>=4.3или gcc>=8.0 
1. openssl dev 1.0.2r
1. pkg-config
1. OpenSSL
1. libuv

## Сборка Unix
```
 $ git clone https://github.com/aithea/capy-amqp-cpp
 $ cp capy-amqp-cpp/tests/broker/dotenv.tmpl capy-amqp-cpp/tests/broker/.env
 $ nano capy-amqp-cpp/tests/broker/.env # and change your rabbitmq address
 $ cd ./capy-amqp-cpp; mkdir -p build; cd build; cmake ..; make -j 4
```

## Сборка osx
```
 $ cd ./capy-amqp-cpp; mkdir -p build; cd build; cmake  -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl ..; make -j 4 
```

## Ускорение сборки за счет предустановленных пакетов
Переменная ``OPENSSL_ROOT_DIR`` может быть использована только для OSX сборок. 

#### Install pkg-config: 
 1. On Linux: ```sudo apt-get install pkg-config```
 1. On OSX: ```brew install pkg-config```

#### Install googletest:
```
$ git clone https://github.com/google/googletest
$ cd googletest; mkdir build; cd build; cmake ..
$ make -j 4; sudo make install
``` 

#### Install capy-dispatchq:
```
$ git clone https://github.com/aithea/capy-dispatchq
$ cd capy-dispatchq; mkdir build; cd build; cmake ..
$ make; sudo make install
```

#### Install rabbitmq-c:
```
$ git clone https://github.com/alanxz/rabbitmq-c
$ cd rabbitmq-c; mkdir build; cd build; cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_EXAMPLES=OFF -DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON -DBUILD_TESTS=OFF -DBUILD_TOOLS_DOCS=OFF -DENABLE_SSL_SUPPORT=OFF ..
$ make; sudo make install
```

#### Install AMQP-CPP:
```
$ git clone https://github.com/dnevera/AMQP-CPP
$ cd AMQP-CPP; mkdir build; cd build; cmake  -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl -DCMAKE_INSTALL_PREFIX=/usr/local -DAMQP-CPP_LINUX_TCP=ON -DAMQP-CPP_BUILD_SHARED=OFF -DAMQP-CPP_BUILD_EXAMPLES=OFF ..
$ make; sudo make install
```

#### Install LibUV
1. On Linux:
 ```bash
 $ sudo add-apt-repository ppa:acooks/libwebsockets6
 $ sudo apt-get update
 $ sudo apt-get install libuv1.dev
 ```
 2. On OSX:
 ```bash
  $ brew install libuv
 ```
 
## Типы данных

1. capy::Error - хендлер ошибок, фреймворк не выбрасывает исключение и не транслирует искоючение из используемых модулей, все исключения перехватываются и хендлятся в capy::Error  
1. capy::json - alias https://github.com/nlohmann/json 
1. capy::Result<capy::json> - alias std::expected<capy::json,Error>
1. capy::amqp::Broker - PFL - прокси к брокеру AMQP

## Создание rpc-сервиса

```c++

    /*
    * Определить адрес брокера, возыращается ожидание типа: std::expected<Broker,Error>
    * (из стандарта c++20), в фреймворке добавлен сахар: capy::Result<json>
    */
    auto address = capy::amqp::Address::From(http://guest:guest@localhost:5672/);

    /*
    * Обработать ошибку адреса
    */
    if (!address) {
        std::cerr << "amqp address error: " << address.error().value() << " / " << address.error().message()
              << std::endl;
        return;
    }
    
    /*
    * Создать соединение с AMQP и вернуть брокера 
    */
    auto broker = capy::amqp::Broker::Bind(address.value());
        
    /*
    * Обработать ошибку создания брокера
    */
    if (!broker) {
        std::cerr << "amqp broker error: " << broker.error().value() << " / " << broker.error().message() << std::endl;
        return ;
    }
    
    /*
    * Слушать очередь "broker-test" с ключами топиков "something.find", "anywhere.thing"
    */
     broker->listen(
                    "capy-test",
                    {"something.find","anywhere.thing"},
                    /* 
                    * Асинхронное замыкание ожидания запроса.
                    * В запрос выбрасывается expected-объект: либо json, либо ошибка запроса (например в момент обработки было разорвано соединение)
                    */
                    [&](const capy::Result<capy::json>& message,
                    /*
                    * Замыкание в случае успеха должно заполнить ответ  
                    */
                        capy::Result<capy::json>& replay)
                    {
    
                        /* проверить сообщение и обработать ощибку */
                        if (!message) {
                          std::cerr << " listen error: " << message.error().value() << "/" << message.error().message() << std::endl;
                        }
                        else {
                          std::cout << " listen["<< counter << "] received: " << message.value().dump(4) << std::endl;
                          replay.value() = {"reply", true, time(0)};
                        }
                    });
  
```

## Создание rpc-клиента 

```c++
    /*
    * Создание адреса и брокера также как и выше
    */
    
    // ...
    
    std::string timestamp = std::to_string(time(0));
    
    /*
    * Конструтор структуры запроса 
    */ 
    capy::json action = {
          {"action", "someMethodSouldBeExecuted"},
          {"payload", {"ids", timestamp}, {"timestamp", timestamp}, {"i", i}}
    };

    /*
    * Отправка action через брокер 
    */
    if (auto error = broker->fetch(action, "something.find",
        /*
        * Асинхронный блок ответа от сервиса 
        */ 
        [&](const capy::Result<capy::json> &message){    
    
        /*
        * Обработка ошибки ответа
        */
        if (!message){        
            std::cerr << "amqp broker fetch receiving error: " << message.error().value() << " / " << message.error().message()
                  << std::endl;        
        }
        else {
            /*
            * Обработка валидного ответа
            */
            std::cout << "fetch["<< i << "] received: " <<  message->dump(4) << std::endl;
        }        
    
    })) {
        
        std::cerr << "amqp broker fetch error: " << error.value() << " / " << error.message()
              << std::endl;
        
    }
```