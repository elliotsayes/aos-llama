#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <emscripten.h>

// WeaveDrive file functions
EM_ASYNC_JS(int, arweave_fopen, (const char* c_filename, const char* mode), {
    try {
        const filename = UTF8ToString(Number(c_filename));
        
        const pathCategory = filename.split('/')[1];
        const id = filename.split('/')[2];
        console.log("JS: Opening file: ", filename);

        if (pathCategory === 'data') {
            if(FS.analyzePath(filename).exists) {
                console.log("JS: File exists: ", filename);
                const file = FS.open("/data/" + id, "r");
                console.log("JS: File opened: ", file.fd);
                return Promise.resolve(file.fd);
            }
            else {
                if (Module.admissableList.includes(id)) {
                    console.log("JS: Getting Arweave ID: ", id);
                    const response = await fetch('https://arweave.net/' + id);
                    
                    // I think we will need to stream download to support larger files
                    // const data = new Int8Array(await response.arrayBuffer());
                    // FS.writeFile('/data/' + id, data);
                    const writer = new WritableStream({
                      write(chunk) {
                        FS.writeFile(`/data/${id}`, new Uint8Array(chunk), { flags: 'a' }); 
                      }
                    });
                    await response.body.pipeTo(writer);
                    console.log("JS: File written!");
                    const file = FS.open("/data/" + id, "r");
                    return Promise.resolve(file.fd);
                }
                else {
                    console.log("JS: Arweave ID is not admissable! ", id);
                    return Promise.resolve(0);
                }
            }
        }
        else if (pathCategory === 'headers') {
            console.log("Header access not implemented yet.");
            return Promise.resolve(0);
        }
        return Promise.resolve(0);
  } catch (err) {
    console.error('Error opening file:', err);
    return Promise.resolve(0);
  }
});

// NOTE: Currently unused, but this is the start of how we would do it.
EM_ASYNC_JS(size_t, arweave_read, (void *buffer, size_t size, size_t nmemb, int fd), {
    try {
        console.log('JS: Reading requested data... ', buffer, size, nmemb, fd);
        console.log("Sending args:", HEAP8, Number(buffer), Number(size) * Number(nmemb), 0);
        const bytes_read = FS.streams[fd].stream_ops.read(FS.streams[fd], HEAP8, Number(buffer), Number(Number(size) * Number(nmemb)), 0);
        console.log('JS: Read data: ', bytes_read);
        await new Promise((r) => setTimeout(r, 1000));
        console.log('Resolving...');
        return Promise.resolve(bytes_read);
    } catch (err) {
        console.error('JS: Error reading file: ', err);
        return Promise.resolve(-1);
    }
});

// NOTE: This may not actually be necessary. But if it is, here is how we would
// emulate the 'native' emscripten fopen.
FILE *fallback_fopen(const char *filename, const char *mode) {
    int fd;
    int flags;

    // Basic mode to flags translation
    if (strcmp(mode, "r") == 0) {
        flags = O_RDONLY;
    } else if (strcmp(mode, "w") == 0) {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (strcmp(mode, "a") == 0) {
        flags = O_WRONLY | O_CREAT | O_APPEND;
    }

    // Open file and convert to FILE*
    fd = open(filename, flags, 0666); // Using default permissions directly
    if (fd == -1) { // If fd is -1, return NULL as if the fopen failed
        return NULL;
    }
    return fdopen(fd, mode);
}

FILE* fopen(const char* filename, const char* mode) {
    fprintf(stderr, "AO: Called fopen: %s, %s\n", filename, mode);
    FILE* res = (FILE*) 0;
    if (strncmp(filename, "/data", 5) == 0 || strncmp(filename, "/headers", 8) == 0) {
        int fd = arweave_fopen(filename, mode);
        fprintf(stderr, "AO: arweave_fopen returned fd: %d\n", fd);
        if (fd) {
            res = fdopen(fd, mode);
        }
    }
    fprintf(stderr, "AO: fopen returned: %p\n", res);
    return res; 
}

int fclose(FILE* stream) {
     fprintf(stderr, "Called fclose\n");
     return 0;  // Returning success, adjust as necessary
}

/*
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    int fd = fileno(stream);
    fprintf(stderr, "AO: fread called with: ptr %p, size: %zu, nmemb: %zu, FD: %d.\n", ptr, size, nmemb, fd);
    size_t bytes_read = arweave_read(ptr, size, nmemb, (unsigned int) fd);
    fprintf(stderr, "I'M BACK\n");
    fflush(stderr);
    //fprintf(stderr, "AO: fread returned: %zu. Output: %s\n", bytes_read, ptr);
    return bytes_read;
}

int fseek(FILE* stream, long offset, int whence) {
    fprintf(stderr, "Called fseek with offset: %ld, whence: %d\n", offset, whence);
    return 0;  // Returning success, adjust as necessary
}

long ftell(FILE* stream) {
    fprintf(stderr, "Called ftell\n");
    return 0;  // Returning 0 as the current position, adjust as necessary
}
*/