# Интерфейсно-Объектная библиотечка fobj - funny objects.

Библиотечка призвана предоставить решение проблеме полиформизма:
- иметь множество реализаций одного поведения (методов),
- и прозрачное инициирование поведения не зависимо от реализации (вызов
  метода).

Реализует концепцию динамического связывания:
- объект аллоцируется со скрытым хедером, содержащим идентификатор класса
- во время вызова метода, реализация метода ищется в рантайме используя
  идентификатор класса из хедера объекта и идентификатор метода.

Плюс, библиотека предоставляет управление временем жизни объектов
посредством счётчика ссылок.

### Пример

Рассмотрим на примере создания объекта с состоянием в виде переменной
double и паре методов: "умножить и прибавить" и "прибавить и умножить".

Без "магии" это могло выглядеть так:
```c
    typedef struct dstate { double st; } dstate;
    double mult_and_add(dstate *st, double mult, double add) {
        st->st = st->st * mult + add;
        return st->st;
    }

    double add_and_mult(dstate *st, double add, double mult) {
        st->st = (st->st + add) * mult;
        return st->st;
    }

    int main (void) {
        dstate mystate = { 1 };
        printf("%f\n", mult_and_add(&mystate, 2, 1));
        printf("%f\n", add_and_mult(&mystate, 2, 1));
        printf("%f\n", mult_and_add(&mystate, 5, 8));
        printf("%f\n", add_and_mult(&mystate, 5, 8));
    }
```

## Основные сущности.

### Метод

Метод - главный персонаж библиотеки. Определяет поведение.

Метод - "дженерик" функция с диспатчингом по первому аргументу.
Первый аргумент не типизирован. Им может быть "произвольный" объект.
Типы второго и следующего аргумента фиксированные.

Аргуметны могут быть "обязательными" и "опциональными". У опционального
аргумента может быть значение по умолчанию.

Диспатчинг происходит в рантайме. Если обнаружится, что на объекте
метод не определён, произойдёт FATAL ошибка с абортом. Если обязательный
аргумент не передан, тоже.

Имена методов строго уникальны, потому именовать метод рекомендуется
с абревиатурой неймспейса. Также рекомендуется использовать
theCammelCase.

Чтобы создать метод, нужно:

- объявить имя, сигнатуру метода.
  Обычно это делается в заголовочных файлах (.h)
  Первый аргумент (объект) явно объявлять не нужно.
```c
    #define mth__hotMultAndAdd double, (double, mult), (double, add)
    #define mth__hotAddAndMult double, (double, add), (double, mult)
    #define mth__hotGetState   double
```
- если есть опциональные аргументы, то добавить их объявление.
  (если нет, то макрос объявлять не нужно)
```c
    #define mth__hotMultAndAdd_optional() (mult, 1), (add, 0)
```
- позвать генерирующий макрос (так же в .h файле, если это метод должен
  быть виден из-вне)
```c
    fobj_method(hotMultAndAdd);
    fobj_method(hotAddAndMult);
    fobj_method(hotGetState);
```

  Макрос генерит следующие объявления, используемые пользователем:

```c
    // Функция вызова метода без "магии" именованных/опциональных
    // параметров.
    static inline double    hotMultAndAdd(fobj_t obj, double mult, double add);

    // Интерфейс для одного метода
    typedef struct hotMultAndAdd_i {
        fobj_t              self;
        hotMultAndAdd_cb    hotMultAndAdd;
    } hotMultAndAdd_i;

    // Биндинг интерфейса для объекта
    static inline hotMultAndAdd_i   bind_hotMultAndAdd(fobj_t obj);

    // Биндинг интерфейса + увеличение счётчика ссылок на объекте.
    static inline hotMultAndAdd_i   bindref_hotAddAndMult(fobj_t obj);
```

  Последующие объявления пользователем непосредственно не используются.
  Можете не читать.

```c
    // Хэндл метода
    static inline fobj_method_handle_t     hotMultAndAdd__mh(void);

    // Тип функции - реализации
    typedef double (*hotMultAndAdd__impl)(fobj_t obj, double mult, double mult);

    // Связанная реализация
    typedef struct hotMultAndAdd_cb {
        fobj_t              self;
        hotMultAndAdd_impl  impl;
    } hotMultAndAdd_cb;

    // Получение связанной реализации
    static inline hotMultAndAdd_cb
    fobj__fetch_hotMultAndAdd(fobj_t obj, fobj_klass_handle_t parent);

    // Регистрация реализации
    static inline void
    fobj__register_hotMultAndAdd(fobj_klass_handle_t klass, hotMultAndAdd_impl impl);

    // Валидация существования метода для класса
    // (В случае ошибки, будет FATAL)
    static inline void
    fobj__klass_validate_hotMultAndAdd(fobj_klass_handle_t klass);

    // Тип для именованных параметров
    typedef struct hotMultAndAdd__params_t {
        fobj__dumb_t    _dumb_first_param;

        double          mult;
        fobj__dumb_t    mult__given;

        double          add;
        fobj__dumb_t    add__given;
    } hotMultAndAdd__params_t;

    // Функция вызова связанной реализации с параметрами.
    static inline double
    fobj__invoke_hotMultAndAdd(hotMultAndAdd__cb cb, hotMultAndAdd__params params);
```

### Интерфейс

Интерфейс - это набор методов.

Служит для двух целей:
- проверки наличия методов на объекте
- небольшого ускорения вызова метода (т.к. реализация метода сохраняется
  в структуре, и вызов не требует рантайм-поиска).

Методы могут быть обязательными и не обязательными.

Для каждого метода сразу создаётся интерфейс, содержащий один обязательный
метод.

Чтобы создать интерфейс, нужно:

- объявить интерфейс
```c
    #define iface__hotState mth(hotMultAndAdd, hotGetState), \
                            opt(hotAddAndMult)
```
  Здесь мы объявили два обязательных и один опциональный метод.
  Количество секций mth и opt - произвольное. Количество методов в них -
  тоже.
  (Произвольное - в пределах разумного - ~ до 16)

- позвать генерирующий макрос
```c
    fobj_iface(hotState);
```

  Макрос генерирует объявления:
```c
    // Структура интерфейса с реализациями методов.
    typedef struct hotState_i {
        fobj_t              self;
        hotMultAndAdd_cb    hotMultAndAdd;
        hotGetState_cb      hotGetState;
        hotAddAndMult_cb    hotAddAndMult;
    } hotState_i;

    // Биндинг интерфейса для объекта
    static inline hotState_i    bind_hotState(fobj_t obj);
    // Биндинг интерфейса + увеличение счётчика ссылок на объекте
    static inline hotState_i    bindref_hotState(fobj_t obj);
```
  И "скрытое объявление"
```
    // Проверка объявления интерфейса
    static inline void
    fobj__klass_validate_hotState(fobj__klass_handle_t klass);
```

### Класс

Класс определяет связывание методов с конкретными объектами.

Объявляя класс, можно указать:
- список методов
- родителя (опционально)
- интерфейсы, которые нужно провалидировать (опционально)
- расширяемая ли аллокация (опционально)

Чтобы создать класс, нужно:
- объявить тело класса
```c
    typedef struct hotDState {
        double st;
    } hotDState;

    // И "наследника"
    typedef struct hotCState {
        hotDState p;        // parent
        double    add_acc;  // аккумулятор для слагаемых
    } hotCState;
```

- обявить сигнатуру класса
```c
    #define kls__hotDState  mth(hotMultAndAdd, hotGetState), \
                            iface(hotState)
    #define kls__hotCState  inherits(hotDState), \
                            mth(hotMultAndAdd, hotAddAndMult),
                            iface(hotState)
```
  Примечания:
  - мы не объявили hotAddAndMult на hotDState, но он удовлетворяет
    hotState, т.к. hotAddAndMult не обязателен.
  - мы не объявляли hotGetState на hotCState, но он удовлетворяет
    hotState, т.к. этот метод он наследует от hotDState.

- позвать генерирующие макросы
```c
    fobj_klass(hotDState);
    fobj_klass(hotCState);
```
  На самом деле, это всего лишь генерит методы хэндл-ов.
```c
    extern fobj_khandle_method_t    hotDState__kh(void);
    extern fobj_khandle_method_t    hotCState__kh(void);
```

- объявить реализации методов:
```c
    static double
    hotDState_hotMultAndAdd(VSelf, double mult, double add) {
        Self(hotDState);
        self->st = self->st * mult + add;
        return self->st;
    }

    static double
    hotDState_hotGetState(VSelf) {
        Self(hotDState);
        return self->st;
    }

    static double
    hotCState_hotMultAndAdd(VSelf, double mult, double add) {
        Self(hotCState);
        // Вызов метода на родителе
        $super(hotMultAndAdd, self, .mult = mult, .add = add);
        self->add_acc += add;
        return self->st;
    }

    static double
    hotCState_hotAddAndMult(VSelf, double add, double mult) {
        Self(hotCState);
        $(hotMultAndAdd, self, .add = add);
        $(hotMultAndAdd, self, .mult = mult);
        return self->st;
    }
```

- После всех реализаций (или хотя бы их прототипов) в одном .c файле нужно создать реализацию хэндла класса:
```c
    fobj_klass_handle(hotDState);
    fobj_klass_handle(hotCState);
```

- Опционально, можно инициализировать класс.
  Этот шаг не обязателен чаще всего, но может потребоваться, если вы собираетесь
  заморозить рантайм (`fobj_freeze`), или захотите поискать класс по имени.
```c
    void
    libarary_initialization(void) {
        /*...*/
        fobj_klass_init(hotDState);
        fobj_klass_init(hotCState);
        /*...*/
    }
```

#### Методы класса

Долго думал, и решил, что нет проблем, решаемых методами класса,
и не решаемых другими методами.

Методы класса играют обычно роли:
- неймспейса для статических функций.
  - Но в С можно просто звать глобальные функции.
- синглтон объектов, связанных с множеством объектов.
  - Если объекту очень нужен синглтон, можно объявить метод,
    возвращающий такой синглтон. Но большинство объектов не требует
    связанного с ним синглтона.
- фабрик для создания объектов
  - Что не отличается от кейса со статическими функциями.

В общем, пока не появится очевидная необходимость в методах класса,
делать их не буду. Ибо тогда потребуется создавать мета-классы и их
иерархию. Что будет серьёзным усложнением рантайма.

### Объекты

Объекты - это экземпляры/инстансы классов.

#### Aллокация.
```c
    hotDState*  dst;
    hotCState*  cst;

    // По умолчанию, аллоцируется, зачищенное нулями.
    dst = $alloc(DState);
    // dst = fobj_alloc(Dstate);

    // Но можно указать значения.
    cst = $alloc(CState, .p.st = 100, .add_acc = 0.1);
    // cst = fobj_alloc(CState, .p.st = 100, .add_acc = 0.1);
```

#### Вызов метода
```c
    // Вызов "без магии"
    printf("%f\n", hotMultAndAdd(dst, 2, 3));
    printf("%f\n", hotMultAndAdd(cst, 3, 4));
    printf("%f\n", hotGetState(dst));
    printf("%f\n", hotGetState(cst));
    printf("%f\n", hotAddAndMult(cst, 5, 6));
    // а вот это свалится с FATAL
    // printf("%f\n", hotAddAndMult(dst, 6, 7));

    // Вызов "с магией"

    // "Классический"
    printf("%f\n", $(hotMultAndAdd, dst, 2, 3));
    printf("%f\n", $(hotMultAndAdd, cst, 3, 4));
    printf("%f\n", $(hotGetState, dst));
    printf("%f\n", $(hotGetState, cst));
    printf("%f\n", $(hotAddAndMult, cst, 5, 6));

    // С именованными параметрами
    printf("%f\n", $(hotMultAndAdd, dst, .mult = 2, .add = 3));
    printf("%f\n", $(hotMultAndAdd, cst, .add = 3, .mult = 4));
    printf("%f\n", $(hotGetState, dst)); // нет параметров.
    printf("%f\n", $(hotAddAndMult, cst, .add = 5, .mult = 6));
    printf("%f\n", $(hotAddAndMult, cst, .mult = 5, .add = 6));

    // С дефолтными параметрами
    printf("%f\n", $(hotMultAndAdd, dst, .mult = 2));
    printf("%f\n", $(hotMultAndAdd, cst, .add = 3));
    printf("%f\n", $(hotMultAndAdd, cst));
    // А вот это упадёт с FATAL, т.к. у hotAddAndMult не имеет
    // опциональных аргументов
    // printf("%f\n", $(hotAddAndMult, cst, .add = 5));
    // printf("%f\n", $(hotAddAndMult, cst, .mult = 5));
    // printf("%f\n", $(hotAddAndMult, cst));
```

Так же можно вызвать метод только если он определён:
```c
    double v;
    if ($ifdef(v =, hotMultAndAdd, dst, .mult = 1)) {
        printf("dst responds to hotMultAndAdd: %f\n", v);
    }
    if (fobj_ifdef(v =, hotAddAndMult, dst, .mult = 1, .add = 2)) {
        printf("This wont be printed: dst doesn't respond to hotAddAndMult");
    }
    if ($ifdef(, hotGetStatus, cst)) {
        printf("cst responds to hotGetStatus.\n"
               "Result assignment could be ommitted. "
               "Although compiler could warn on this.");
    }
```

#### Биндинг метода/интерфейса

По сути, это всегда биндинг интерфейса. Просто каждый метод определяет
интерфейс с одним этим методом.

```c
    hotMultAndAdd_i  hmad = bind_hotMultAndAdd(dst);
    hotMultAndAdd_i  hmac = bind_hotMultAndAdd(cst);
    hotState_i       hstd = bind_hotState(dst);
    hotState_i       hstc = bind_hotState(cst);
```

### Вызов метода на интерфейсе

Заметьте, тут интерфейс передаётся по значению, а не по поинтеру.
Сделал так после того, как один раз ошибся: вместо `$i()` написал `$()`,
и компилятор радостно скомпилировал, т.к. `$()` принимает `void*`.

```c
    printf("%f\n", $i(hotMultAndAdd, hmaa, .mult = 1));
    printf("%f\n", $i(hotMultAndAdd, hmac, .add = 2));

    printf("%f\n", fobj_iface_call(hotMultAndAdd, hmaa, .add = 4));
    printf("%f\n", fobj_iface_call(hotMultAndAdd, hmac));

    printf("%f\n", $i(hotMultAndAdd, hstd));
    printf("%f\n", $i(hotMultAndAdd, hstc));
    printf("%f\n", $i(hotGetState, hstd));
    printf("%f\n", $i(hotGetState, hstc));

    printf("%f\n", $i(hotAddAndMult, hstc, .mult = 4, .add = 7));
    // Проверка на обязательность аргументов тут работает так же.
    // Потому след.вызовы упадут с FATAL:
    // $i(hotAddAndMult, hstd, .mult = 1);
    // $i(hotAddAndMult, hstd, .add = 1);
    // $i(hotAddAndMult, hstd);
```

A вот на `hstd` так просто `hotAddAndMult` позвать нельзя:
- `hotDState` этот метод не определял
- `hotAddAndMult` является опциональным методом интерфейса
- потому в `hstd` этот метод остался не заполненным.
Нужно проверять:

```c
    if ($ifilled(hotAddAndMult, hstd)) {
        printf("This wont be printed: %f\n",
                $i(hotAddAndMult, hstd, .mult=1, .add=2));
    }
    if (fobj_iface_filled(hotAddAndMult, hstd)) { /*...*/ }
```

Или воспользоваться условным вызовом метода:
```c
    if ($iifdef(v =, hotAddAndMult, hstd, .mult = 1, .add = 2)) {
        printf("This wont be printed: %f\n", v);
    }
```

#### Проверка реализации интерфейса

Вызов `bind_someInterface` упадёт с FATAL, если интерфейс не реализован.

Проверить, реализован ли интерфейс, можно с помощью
`$implements()`/`implements_someInterface()` :

```c
    if ($implements(hotState, dst)) {
        workWithObject(dst);
    }
    if (fobj_implements(hotState, cst)) {
        workWithObject(cst);
    }
    if ($implements(hotState, dst, &hstd)) {
        $i(hotGetState, hstd);
    }
    if (fobj_implements(hotState, cst, &hstc)) {
        $i(hotGetState, hstc);
    }
    // И без макросов
    if (implements_hotState(dst, NULL)) {
        workWithObject(dst);
    }
    if (implements_hotState(cst, &hstc)) {
        $i(hotGetState, hstc);
    }
```

### Время жизни.

Время жизни объекта управляется методом подсчёта ссылок.
Когда счётчик доходит до 0, объект уничтожается.

Непосредственная работа со счётчиком:
```c
    fobj_t v, obj;
    // Увеличить счётчик на объекте - "Удержать объект"
    $ref(obj);
    fobj_retain(obj);
    // Увеличить счётчик на объекте, возвращённом из метода.
    store->field = $ref($(createObject, fabric));
    store->field = fobj_retain(createObject(fabric));

    // Уменьшить счётчик - "Освободить объект"
    fobj_release(obj);
    // $del так же присваивает переменной NULL
    $del(&v);
    fobj_del(&v);
```

Перезапись значения переменной может быть не простой:
- новому значению нужно увеличить счётчик
- старому значению нужно уменьшить счётчик
- причём именно в таком порядке: сперва увеличить, потом уменьшить, т.к.
  это может быть один и тот же объект.

Потому сделан макрос
```c
    // Перезаписать переменную
    $set(&v, obj);
    fobj_set(&v, obj);

    // Перезаписать переменную возвращённым объектом
    $set(&v, $(createObject, fabric));
    fobj_set(&v, createObject(fabric));

    // Перезаписать поле
    $set(&store->field, obj);
    fobj_set(&store->field, $(createObject, fabric));
```

#### Деструктор

Когда объект уничтожается, выполняется стандартный метод `fobjDispose`
(если определён на объекте).
```c
    typedef struct myStore {
        fobj_t field;
    } myStore;

    #define kls__myKlass mth(fobjDispose)

    static void
    myKlass_fobjDispose(VSelf) {
        Self(myKlass);
        $del(&self->field);
    }
```

Методы `fobjDispose` вызываются по всех классов в цепочке наследования,
для которых они определены, по порядку от потомков к первому родителю.

Нет способа вернуть ошибку из `fobjDispose` (т.к. не куда). Он обязан
сам со всем разобраться.

Иногда нужно "отключить объект" не дожидаясь, пока он сам уничтожится.
Т.е. хочется позвать `fobjDispose`. Но явный вызов `fobjDispose` запрещён.
Для этого нужно воспользоваться обёрткой `fobj_dispose`.

Обёртка гарантирует, что `fobjDispose` будет позван только один раз.
Кроме того, она запоминает, что вызов `fobjDispose` завершился, и после
этого любой вызов метода на этом объекте будет падать в FATAL.

### AutoRelease Pool (ARP)

Для облегчения жизни программиста, используется концепция AutoRelase Pool:
- в начале крупных функций и в теле долгих циклов создаётся пул
```c
    fobj_t
    longRunningFunction()
    {
        FOBJ_FUNC_ARP();
        /*...*/
        for (i = 0; i < 1000000000; i++) {
            FOBJ_LOOP_ARP();
            /*...*/
        }
    }
```
- также можно объявить пул для блока, если нужно ограничить время жизни
  локальных объектов
```c
        /*...*/
        {
            FUNC_BLOCK_ARP();
            /*...*/
        }
```
- когда программа выходит из блока, имеющего ARP, всем объектам,
  помещённым в ARP, выполняется `fobj_release` столько раз, сколько раз
  объект помещён в пул.
- все вновь созданные объекты имеют refcnt = 1 и уже помещены в
  "ближайший" ARP.
- все объекты, возвращённые методами/функциями, должны либо принадлежать
  объектам, возвращающим их, либо помешены в "ближайший" ARP пул.

Благодаря этим правилам, чаще всего объекты не нужно явно "освобождать".

Звать "удержания"/"освобождения" нужно в основном связывая объекты между
собою.
- `$ref` (`fobj_retain`) в основном нужно звать в методах-присваиваниях и
  во время аллокации объекта при передаче значений.
```c
    #define mth__mySetField fobj_t, (fobj_t, val)

    #define kls__myKlass mth(fobjDispose, mySetField)

    static fobj_t
    myKlass_mySetField(VSelf, fobf_t val) {
        Self(myKlass);
        $set(&self->field, val);
        return val;
    }

    // Конструктор бедного человека :)
    fobj_t
    createKlass(fobj_t inner) {
        fobj_t state;
        state = $alloc(myKlass, .field = $ref(inner));
        return state;
    }
```

Вручную `$del` (`fobj_del`, `fobj_release`) нужно звать в основном в
`fobjDispose`.

Однако, может потребоваться "сохранять" объекты от уничтожения при выходе
из блока/функции.

```c
    // Нужно вернуть объект
    fobj_t
    doSomethingAndReturn(/*...*/, fobjErr **err) {
        FOBJ_FUNC_ARP(); // AutoRelease Pool для функции
        fobj_t result;
        fobj_t loop_result = NULL;
        // Проверим, что err != NULL, и присвоим *err = NULL
        fobj_reset_err(err);

        for(/*...*/) {
            FOBJ_LOOP_ARP(); // AutoRelease Pool для каждой итерации цикла
            fobj_t some = findsomewhere(/*...*/);

            if (isGood(some)) {
                // Если не сделать $save(some), то он (возможно)
                // уничтожится при выходе из цикла.
                loop_result = $save(some);
                break;
            }
            if (tooBad(some)) {
                // нужно "вернуть" err
                *err = fobj_error("SHIT HAPPENED");
                // Без этого *err будет уничтожен при выходе из функции
                $returning(*err);
                return NULL;
            }
        }

        result = createKlass(loop_result);
        // return $returning(result);
        // Или короче
        $return(result);

        // Если сделать просто `return result`, то объект уничтожится
        // при выходе из функции.
    }
```

Можно вручную помещать объекты в AR пул. Например, это может
потребоваться при возврате объекта из метода с потерей связи между
объектами.

```c
    #define mth__myPopField         fobj_t
    #define mth__myReplaceField     fobj_t, (fobj_t, newVal)
    #define kls__myKlass /*...*/, mth(myPopField, myReplaceField)

    static fobj_t
    myKlass_myPopField(VSelf) {
        Self(myKlass);
        fobj_t val = self->field;
        self->field = NULL;
        // val будет помещён в ближайший ARP.
        // Если ни кто не "удержит" объект, он будет уничтожен.
        return $adel(val);
        // return fobj_autorelease(val);
    }

    static fobj_t
    myKlass_myReplaceField(VSelf, fobj_t newVal) {
        Self(myKlass);
        fobj_t oldVal;

        if (rand()&1) {
            // Аналогично c $adel
            oldVal = self->field;
            self->field = $ref(newVal);
            return $adel(val);
            // Или
            // self->field = fobj_retain(newVal);
            // return fobj_autorelease(oldVal);
        } else {
            // Или с $aref
            oldVal = $aref(self->field);
            $set(&oldVal, newVal);
            return oldval;
            // Без баксов:
            // oldVal = fobj_autorelease(fobj_retain(self->field));
            // fobj_set(&self->field, newVal)
            // return oldVal;
        }
    }
```

Так же это можно использовать, для быстрого выхода из вложенных ARP пулов:

```c
    fobj_t
    doSomethingAndReturn(/*...*/) {
        FOBJ_FUNC_ARP(); // AutoRelease Pool для функции
        fobj_t result;
        fobj_t loop_result = NULL;

        {
            FOBJ_BLOCK_ARP();
            /*...*/
            for (/*...*/) {
                FOBJ_LOOP_ARP();
                /*...*/
                if (/*...*/) {
                    loop_result = $ref(some);
                    goto quick_exit;
                }
            }
        }
    quick_exit:
        // Не забыть поместить в ARP
        $adel(loop_result)
        /*...*/
    }
```

### Конструктор

И тут вероятно вы зададите мне вопрос:
- Эй, уважаемый, что за омлет? А где же яйца?
    (с) Дискотека Авария.

В смысле: до сих пор ни слова про конструкторы.

Вернее, было одно слово в примере кода: "Конструктор бедного человека".

И в целом, это описывает подход к конструкторам в этой библиотеке:
- конструированием объекта должен заниматься не объект.
- объект конструирует либо глобальная функция, либо метод другого объекта.
  - либо всё нужное для работы объекта готовится перед аллокацией и
    передаётся непосредственно в значения полей в момен аллокации,
  - либо у объекта есть некий метод `initializeThisFu__ingObject`
    который зовётся после аллокации.
    (название выдуманно, такого стандартного метода нет),

Этот подход применён в Go, и в целом, его можно понять и принять.
Сделать семантику конструктора с корректной обработкой возможных ошибок,
вызова родительского конструктора и прочим не просто. И не сказать,
чтобы языки, имеющие конструкторы, справляются с ними без проблем.

В библиотечке так же наложились проблемы:
- с сохранением где-нибудь имени метода(?) или функции(?) конструктора
  и передачи ему параметров.
- перегрузки конструкторов в зависимости от параметров(?)
- необходимость уникальных имён методов для каждого набора параметров.
- необходимость куда-то возвращать ошибки?
- отсутствие методов класса.

В общем, пораскинув мозгами, я решил, что простота Go рулит, и усложнять
это место не особо нужно.
Тем более, что зачастую объекты создаются в методах других объектах.

На что нужно обратит внимание:
- если вы пользуетесь передачей начальных значений в `$alloc`
- и освобождаете что-то в `fobjDispose`
- то передавать в `$alloc` нужно то, что можно в `fobjDispose` освободить.

Т.е.
- если вы передаёте объект, то его нужно `$ref(obj)`
- если вы передаёте строку, то её нужно `ft_strdup(str)`
- и т.д.

```c
    typedef struct myKlass2 {
        fobj_t someObj;
        char*  someStr;
    } myKlass2;

    #define mth__initMyKlass2 void, (fobj_t, so), (char*, ss)

    #define kls__myKlass2 mth(fobjDispose, initMyKlass2)

    void
    myKlass2_initMyKlass2(VSelf, fobj_t so, char* ss) {
        Self(myKlass2);
        const char *old = self->someStr;
        $set(&self->someObj, so);
        self->someStr = ft_strdup(ss);
        ft_free(old);
    }

    myKlass2_fobjDispose(VSelf) {
        Self(myKlass2);
        $del(&self->someObj);
        ft_free(self->someStr);
    }

    myKlass2*
    make_MyKlass2(fobj_t so, char *ss) {
        return $alloc(myKlass2,
                      .someObj = $ref(so),
                      .someStr = ft_strdup(ss));
    }

    myKlass2*
    make_alloc_MyKlass2(fobj_t so, char *ss) {
        return $alloc(myKlass2,
                      .someObj = $ref(so),
                      .someStr = ft_strdup(ss));
    }

    myKlass2*
    make_set_MyKlass2(fobj_t so, char *ss) {
        MyKlass2* mk = $alloc(myKlass2);
        mk->someObj = $ref(so);
        mk->someStr = ft_strdup(ss);
        return mk;
    }

    myKlass2*
    make_init_MyKlass2(fobj_t so, char *ss) {
        MyKlass2* mk = $alloc(myKlass2);
        initMyKlass2(mk, so, ss);
        return mk;
    }
```

### Инициализация

В главном исполняемом файле где-нибудь в начале функции `main` нужно позвать:
```c
    fobj_init();
```
До этого момента создание новых классов (`fobj_klass_init`) будет падать с
FATAL ошибкой.
