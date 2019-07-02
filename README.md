# Capy RabbitMQ wrap library for C++

Acинхронный клиент к брокеру сообщений реализующий шаблон PFL - Publish/Fetch/Listen. 
Размер пула потоков задается автоматически в зависимости от количества физических ядер хоста.  
Доставка и получение собщений к брокеру остается асинхронной и не блокирует клиентское приложение.

***TODO***: настройка параметров соединения (таймауты и тп...., кстомный размер пула асинхронных вызывов)

## Зависимости
1. https://github.com/dnevera/AMQP-CPP - низкоуровневый фсинхронный API к RabbitMQ на базе libuv
1. https://github.com/aithea/capy-dispatchq - обертка к thread-pool
1. https://github.com/google/googletest - тесты, понятное дело
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
 $ nano capy-amqp-cpp/tests/broker/.env # and change your RabbitMQ address
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
     int error_state = static_cast<int>(capy::amqp::CommonError::OK);
    
      do {
        
        capy::Result<capy::amqp::Broker> broker = capy::amqp::Broker::Bind(*address);
    
        EXPECT_TRUE(broker);
    
        if (!broker) {
          //
          // Процессинг ощибки биндинга брокера с обменником AMQP
          // broker.error().value() / broker.error().message()
          //
          return;
        }
    
        std::promise<int> error_state_connection;
    
        broker->listen("capy-test", {"something.find","anywhere.thing"})
    
                .on_data([](const capy::amqp::Request &request, capy::amqp::Replay &replay) {
    
                    if (request) {
                        //
                        // процессинг корректного результата запроса
                        //  
                        // request->message.dump() / request->routing_key
                        //    
                        
                        ...                                            
                        
                        //
                        // Сформировать либо ответ либо ошибку
                        //
                        
                        if (/* какая-то логическая ощибка формирования ответа */) {
                            replay = capy::make_unexpected(capy::Error(
                                                    capy::amqp::BrokerError::DATA_RESPONSE,
                                                    capy::error_string("some error...")));
                         }
                         else {
                            //
                            // Запрос корректо обработан отсылаем данные
                            //
                            replay.value() = {"data", "some data..."};
                         }              
                     
                    } else {
                        //
                        // Процессинг некорректного запроса
                        //
                            
                        capy::workspace::Logger::log->critical("Router: amqp broker error: {}", request.error().message());

                        replay = capy::make_unexpected(request.error());
                    }
                })
    
                .on_success([] {
                    //
                    // Препроцессинг успешного формирования процесса обработки
                    //    
                })
    
                .on_error([&error_state_connection](const capy::Error &error) {
                    
                    //
                    // Пример восстановления биндинга с обменнником при возникновении системной ощибки
                    // (но моюно и отвалиться... )
                    //
                    try {
                      error_state_connection.set_value(static_cast<int>(error.value()));
                    }catch (...){}
    
                });
    
    
        error_state = error_state_connection.get_future().get();
        
      } while (error_state != static_cast<int>(capy::amqp::CommonError::OK));
                        
  
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
    * Асинхронная отправка action через брокер 
    */
    broker->fetch(action, "somthing.find")
   
               .on_data([](const capy::amqp::Response &response){
   
                   if (response){
                        //
                        // процессинг корректного результата ответа
                        //  
                        // response->dump() / response.value().dump()...
                        //                   
                   }
                   else {
                        //
                        // процессинг ошибки ответа
                        //  
                        // response.error().value() / response.error().message()
                        //
                   }
  
               })
   
               .on_error([](const capy::Error& error){
                        //
                        // процессинг системной ощибки: потеря соединения,
                        // лимит коннектов, каналов, размера очереди и тп... 
                        // - error().value() / error().message()
                        //                   
               });
```