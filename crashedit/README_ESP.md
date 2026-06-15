# CrashEdit - Guía de Flujo de Uso Completo

## Introducción

CrashEdit es un lector y editor de mensajes FTN (FidoNet) que te permite leer áreas de mensajes (echomail), correo privado (netmail), escribir nuevos mensajes y responder a los existentes. Funciona internamente en UTF-8.

---

## Inicio del Programa

### 1. Lanzamiento

Al iniciar CrashEdit, el programa busca un archivo de configuración:

- Si ejecutas `./crashedit` sin argumentos, busca `crashedit.conf` en el directorio actual
- Si especificas un archivo: `./crashedit /ruta/a/miconf.conf`

**Primera ejecución:** Si no existe configuración, se abre automáticamente la pantalla de configuración (Setup) donde deberás definir:
- Tu nombre (SYSOP)
- Al menos una dirección AKA (tu dirección FidoNet) (Las que podrás usar en netmail, son las que tienes en el área golded)
- El archivo de áreas (AREAFILE) generado por crashexport

### Pantalla Inicial: Lista de Áreas

Una vez configurado, CrashEdit muestra la **Lista de Áreas** - todas las áreas de mensajes disponibles organizadas en una tabla.
Si no puedes acceder a ellas, debes verificar que el tosser ha creado los archivos.

**Qué ves:**
- Número de área, nombre corto (tag), descripción
- Total de mensajes y mensajes nuevos no leídos

**Qué puedes hacer:**
- Navegar con flechas arriba/abajo
- Buscar área por nombre con `/`
- Cambiar orden con `s`
- Entrar en un área con `Enter`

---

## Escribir un Mensaje Nuevo

Se abre el **Editor** con el cursor en el campo "To" (Para):

### Rellenar los Campos de Cabecera

**Navegación entre campos:**
- `Tab` / `↓` / `Ctrl+N`: Siguiente campo
- `↑` / `Ctrl+P`: Campo anterior
- `Enter` en cualquier campo de cabecera: Ir directamente al cuerpo
- `Ctrl+N` o `Ctrl+P` si estás en el body, te envía de nuevo a la cabecera

1. **To (Para):** Escribe el nombre del destinatario o `All` para todos
2. **Subj (Asunto):** Escribe el tema del mensaje
3. Pulsa `Enter` o `Tab` para ir al cuerpo

**Nota para Netmail:**
- `Ctrl+A` o `F4`: Cambiar AKA (dirección de envío). Muestra lista de AKAs configurados en areas.golded

### Escribir el Cuerpo y Guardar

Usa las teclas del **Editor** (ver sección "Uso del Editor" más abajo) para escribir tu mensaje.

Cuando termines, presiona **`F2`** o **`Ctrl+S`** para guardar.

**Nota:** CrashEdit guarda el mensaje pero no lo envía directamente. Para preparar el envío, usa el tosser y luego el mailer hará lo propio.

---

## Funciones Especiales de Netmail

### CC (Carbon Copy) - Copias a Múltiples Destinatarios

Para enviar copias de un mensaje a múltiples destinatarios, escribe líneas `CC:` en el cuerpo del mensaje:

```
CC: 2:341/207@fidonet
CC: 39:190/101@amiganet
CC: Blabla 2:341/201@fidonet 
```

**Formato:**
- **Solo dirección:** `CC: 2:341/207@fidonet` → La copia va al mismo nombre del campo **To** pero en esa dirección FTN
- **Con nombre:** `CC: Blabla 2:341/201@fidonet` → La copia va a **Blabla** en esa dirección FTN

CrashEdit convertirá estas líneas en kludges `^ACC:` y el mensaje se enviará a todos los destinatarios especificados. Esto es útil para enviar el mismo mensaje a varios nodos sin escribir mensajes separados.

### File Request (FREQ) - Solicitar Archivos vía FidoNet

Desde la lista de áreas o mensajes, presiona **`Ctrl+F`** para abrir la pantalla de File Request.

**Pasos:**
1. Introduce la dirección FTN del nodo que tiene los archivos
2. Selecciona el modo de outbound:
   - **ASO** (flat): Todos los archivos `.req` en un solo directorio
   - **BSO** (BinkleyStyle): Organizado por zonas
   - **BSO+ext**: BSO con extensión de zona hexadecimal
3. Añade archivos a solicitar con **`A`**
4. (Opcional) Establece contraseña con **`P`**
5. Genera la solicitud con **`W`** (write)

**Requisito:** Debes tener configurado `OUTBOUND` en tu `crashedit.conf` apuntando al directorio outbound de tu mailer.

El archivo `.req` generado será procesado por tu mailer y enviado al nodo destino, que responderá enviando los archivos solicitados.

### Adjuntos de Archivos (File Attach)

En mensajes **netmail**, puedes adjuntar archivos para enviarlos a otros nodos:

**En el editor:**
- **`Ctrl+F`**: Añadir archivo adjunto (selecciona archivo del disco)
- **`Ctrl+Q`**: Quitar adjunto seleccionado
- **`Ctrl+L`**: Listar todos los adjuntos
- **`Alt+L`**: Limpiar todos los adjuntos

**Notas importantes:**
- Máximo 32 archivos adjuntos por mensaje
- Los nombres de archivo aparecen en la línea de asunto (limitado a 71 carácteres por FTS-1)
- Si hay muchos archivos, el mensaje se divide automáticamente en varios
- Los adjuntos se envían como mensajes con el atributo `FileAttach`

**Para recibir archivos adjuntos:**
El mailer recibirá los archivos y los colocará en tu directorio de inbound. El mensaje en CrashEdit mostrará los nombres de archivo en el asunto.

---

## Lector de Mensajes

Al entrar en un área y seleccionar un mensaje, se abre el **Lector** donde puedes ver el contenido completo.

### Navegación en el Lector

- **`↑` / `↓`** o **`PgUp`** / **`PgDn`**: Moverse por el texto
- **`Space`**: Página siguiente
- **`Home`**: Ir al inicio del mensaje
- **`End`**: Ir al final
- **`→`** / **`←`**: Cambiar de mensaje (anterior/siguiente)
- **`Enter`**: Volver a la lista de mensajes

### Modo ANSI (Colores)

Los mensajes pueden contener códigos de color ANSI (códigos de escape para colores y efectos).

- **`a`**: Alternar modo ANSI (activar/desactivar colores)
- Cuando ANSI está **activo**: Los códigos de color se interpretan y el texto se muestra con colores
- Cuando ANSI está **desactivo**: Los códigos se muestran como texto plano (ej: `[0;32m`)

Esto es útil si un mensaje tiene códigos ANSI que dificultan la lectura o si quieres ver el texto sin formato.

### Mostrar/Ocultar Elementos

- **`k`**: Mostrar/ocultar kludges (líneas de control como ^A MSGID, SEEN-BY, Via)
- **`v`**: Alternar vista de kludges y líneas ocultas

### Cambiar Codificación (Charset)

- **`c`**: Cambiar el charset de visualización (LATIN-1, CP437, UTF-8, etc.)
- Permite ver mensajes escritos en codificaciones diferentes correctamente

### Modos de Codificación (Charsets)

CrashEdit soporta múltiples codificaciones de carácteres para mensajes FTN:

**Charsets disponibles:**
- **CP437** (default para lectura): Codepage 437, usado tradicionalmente en DOS/FTN
- **CP850**: Codepage 850 (multilingüe DOS)
- **CP865**: Codepage 865 (Nórdico)
- **CP866**: Codepage 866 (Cirílico)
- **LATIN-1** (ISO-8859-1): Europeo Occidental
- **LATIN-2** (ISO-8859-2): Europeo Central
- **CP1252**: Windows-1252
- **UTF-8** (default para escritura): Unicode, soporte completo de carácteres

**Modo AUTO (Visualización):**
- Cuando el charset de visualización está en **Auto**, CrashEdit detecta automáticamente la codificación del mensaje
- Busca el kludge `^ACHRS:` o `^ACHARSET:` en el mensaje
- Si no encuentra kludge, usa CP437 por defecto
- Esto permite leer mensajes antiguos automáticamente sin configuración manual

**Dos Charsets Independientes:**

1. **View Charset (Visualización)**: Cómo se muestra el mensaje en pantalla
   - Auto = detectar del mensaje
   - Manual = forzar una codificación específica

2. **Save/Output Charset (Guardado)**: Cómo se guardan los mensajes que escribes
   - Configurable en Setup
   - UTF-8 por defecto para máxima compatibilidad moderna
   - Puedes cambiarlo a CP437 para compatibilidad con sistemas antiguos

**Cuándo usar modo Manual:**
- Si un mensaje se ve mal (carácteres extraños), prueba cambiar manualmente a LATIN-1 o CP850
- Para mensajes en cirílico, usa CP866
- Si el modo Auto falla en detectar correctamente

---

## Responder a un Mensaje

En el lector, presiona **`r`** (reply).

El **Editor** se abre con datos pre-rellenados:
- El tema se copia precedido por "Re:"
- El destinatario se copia del remitente original
- El texto original se cita con `>` al inicio de cada línea

Escribe tu respuesta debajo del texto citado y presiona **`F2`** o **`Ctrl+S`** para guardar.

---

## Editar un Mensaje Propio

Navega a un mensaje que tú hayas escrito (aparece tu nombre en "From") y presiona **`e`** (edit).

El **Editor** se abre con el mensaje cargado. Haz los cambios necesarios y guarda con **`F2`** o **`Ctrl+S`**.

**Nota:** Solo puedes editar tus propios mensajes. El programa verifica que el remitente coincida con tu AKA configurada.

---

## Borrar Mensajes

### Opción A: Desde la Lista de Mensajes
1. Selecciona el mensaje con las flechas
2. Presiona **`d`** (delete)
3. Confirma con `Y` cuando se solicite

### Opción B: Desde el Lector
1. Lee el mensaje
2. Presiona **`d`** o **`Del`**
3. Confirma con `Y`

**El mensaje no se borra físicamente** - se marca para borrado y se eliminará durante el mantenimiento de la base de mensajes (purga).

---

## Catch-up (Marcar como Leído)

Cuando hayas leído lo que te interesa de un área y quieras marcar el resto como leído:

### Desde la Lista de Áreas
1. Selecciona el área
2. Presiona **`C`** o **`Alt+C`**
3. Selecciona `Y` para esta área o `A` para todas

### Desde la Lista de Mensajes
Presiona **`C`** - marcará todos los mensajes del área actual como leídos.

---

## Búsqueda de Mensajes

### Búsqueda Rápida (From/To/Subj)
Desde la lista de mensajes:
1. Presiona **`/`**
2. Escribe el texto a buscar
3. Usa `n` para siguiente coincidencia

### Búsqueda Completa (Headers + Body)
Desde la lista de mensajes o áreas:
1. Presiona **`P`**
2. Aparece el popup de búsqueda:
3. Configura opciones:
   - `Space` en una casilla para activar/desactivar
   - Patrón de búsqueda obligatorio
4. `Enter` para ejecutar

**Resultados:** Se muestra lista de áreas con coincidencias. Selecciona un área para ver los mensajes encontrados.

---

## Uso del Editor

El editor se utiliza para escribir y modificar mensajes. Se accede desde:
- **Escribir nuevo mensaje**: Desde la lista de áreas con `N` o `Ins`
- **Responder**: Desde el lector con `r`
- **Editar mensaje propio**: Desde la lista de mensajes o lector con `e`

### Edición Básica

- **`Enter`**: Nueva línea
- **`Backspace`** / **`Delete`**: Borrar carácteres
- **`Ctrl+K`** / **`Ctrl+Y`**: Borrar línea completa
- **`Ctrl+Z`**: Deshacer último cambio
- **`Alt+Z`**: Rehacer (redo)
- **`F2`** / **`Ctrl+S`**: Guardar mensaje
- **`F10`** / **`Esc`** / **`Ctrl+Q`**: Cancelar edición

### Navegación entre Campos (Cabecera)

- **`Tab`** / **`↓`** / **`Ctrl+N`**: Siguiente campo (To → Subj → Body)
- **`↑`** / **`Ctrl+P`**: Campo anterior
- **`Enter`** en cabecera: Ir al cuerpo directamente
- **`Ctrl+N`** o **`Ctrl+P`** estando en el body: Volver a la cabecera

### Selección y Bloques (F6)

**Marcar un bloque:**
1. Coloca el cursor donde inicia la selección
2. Presiona **`F6`** - aparece indicador de bloque activo
3. Mueve el cursor al final de la selección
4. El texto entre ambas posiciones se resalta

**Operaciones con bloque:**
- **`Ctrl+C`**: Copiar bloque (también al portapapeles del sistema)
- **`Ctrl+X`**: Cortar bloque (también al portapapeles)
- **`Ctrl+V`**: Pegar bloque (o desde portapapeles si no hay bloque interno)
- **`Delete` o `Backspace`**: Borrar bloque seleccionado
- **`Ctrl+O`**: Exportar bloque a archivo

### Portapapeles del Sistema

CrashEdit se integra con el portapapeles del sistema operativo:

- **Copiar (`Ctrl+C`)**: El texto seleccionado se copia tanto al portapapeles interno como al del sistema. Puedes pegarlo en otras aplicaciones.
- **Cortar (`Ctrl+X`)**: Igual que copiar, pero borra el texto del editor.
- **Pegar (`Ctrl+V`)**: Primero intenta pegar desde el portapapeles interno (bloque marcado con F6). Si no hay bloque interno, pega desde el portapapeles del sistema.

Esto permite intercambiar texto entre CrashEdit y otras aplicaciones (navegador, editor de texto, etc.).

### Buscar y Reemplazar

- **`F5`**: Buscar texto hacia adelante
- **`Ctrl+R`**: Buscar y reemplazar interactivo
- **`Ctrl+G`**: Ir a número de línea específico

### Deshacer/Rehacer

- **`Ctrl+Z`**: Deshacer último cambio
- **`Alt+Z`**: Rehacer (redo)

### Ajustar Párrafo

- **`Ctrl+W`**: Reajustar párrafo a 75 columnas (útil para citas)

### Insertar Archivo

- **`F7`**: Insertar contenido de un archivo desde disco

### Kludges (Líneas de Control FTN)

- **`F8`**: Ver y editar las líneas de control (^A MSGID, REPLY, etc.)

### Adjuntos (Solo Netmail)

- **`Ctrl+F`**: Añadir archivo adjunto
- **`Ctrl+Q`**: Quitar adjunto
- **`Ctrl+L`**: Listar adjuntos
- **`Alt+L`**: Limpiar todos los adjuntos

---

## Configuración (Setup)

Accede a la configuración desde cualquier pantalla principal con **`S`**.

### Opciones Configurables

**Datos Personales:**
- Nombre del SysOp
- Direcciones AKA (múltiples)

**Archivos:**
- AREAFILE (lista de áreas)
- Charset por defecto

**Preferencias:**
- Codificación de carácteres (LATIN-1, CP437, UTF-8)
- Origen (Origin) para mensajes
- Max hits en búsquedas

**Opciones de Visualización:**
- Colores y tema
- Margen de visualización

---

## Cierre del Programa

### Desde la Lista de Áreas
Presiona **`q`**. Si no hay operaciones pendientes, el programa se cierra inmediatamente.

### Desde Cualquier Otra Pantalla
Presiona **`q`** o **`Esc`** repetidamente hasta volver a la lista de áreas, luego **`q`** para salir.

### Desde el Editor
- **`F10`**, **`Esc`**, o **`Ctrl+Q`**: Cancelar edición
- Si hay cambios sin guardar, se solicitará confirmación:
  - `Y` = Descartar cambios y salir
  - `N` = Volver al editor

---

## Consejos Útiles

1. **Siempre usa `?` o `F1`** en cualquier pantalla para ver las teclas disponibles
2. **Catch-up frecuente**: Usa `C` regularmente para mantener el contador de mensajes nuevos bajo control
3. **Charset**: Si ves carácteres extraños, usa `Alt+H` en el editor para cambiar la codificación (F3 también funciona pero tiene comportamiento contextual en modo búsqueda)
4. **Búsqueda**: Usa `P` para buscar en el cuerpo de los mensajes (más lento pero completo)

---

## Fin

Para más detalles sobre configuración avanzada, consulta el archivo `crashedit.conf.example`.
