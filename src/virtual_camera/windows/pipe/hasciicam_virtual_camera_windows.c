#include "hasciicam_virtual_camera_windows.h"

#include "../../virtual_camera_internal.h"
#include "hasciicam_virtual_camera_pipe.h"

#include <stdlib.h>
#include <windows.h>

typedef struct hasciicam_virtual_camera_windows_state {
    HANDLE pipe_handle;
    HANDLE accept_thread;
    HANDLE stop_event;
    LONG connected;
    unsigned long long sequence;
    hasciicam_virtual_camera_request request;
    char pipe_name[256];
    unsigned char *message_buffer;
    size_t message_size;
    size_t payload_size;
} hasciicam_virtual_camera_windows_state;

static DWORD WINAPI windows_accept_thread(LPVOID param) {
    hasciicam_virtual_camera_windows_state *state =
        (hasciicam_virtual_camera_windows_state *)param;

    if (state == NULL || state->pipe_handle == INVALID_HANDLE_VALUE)
        return 0;
    while (WaitForSingleObject(state->stop_event, 0) != WAIT_OBJECT_0) {
        BOOL connected = ConnectNamedPipe(state->pipe_handle, NULL);
        if (!connected) {
            DWORD err = GetLastError();
            if (err != ERROR_PIPE_CONNECTED && err != ERROR_NO_DATA) {
                Sleep(50);
                continue;
            }
        }
        InterlockedExchange(&state->connected, 1);
        while (WaitForSingleObject(state->stop_event, 0) != WAIT_OBJECT_0) {
            if (WaitForSingleObject(state->stop_event, 25) == WAIT_OBJECT_0)
                break;
            if (InterlockedCompareExchange(&state->connected, 1, 1) != 1)
                break;
        }
        DisconnectNamedPipe(state->pipe_handle);
        InterlockedExchange(&state->connected, 0);
    }
    return 0;
}

static int windows_publish(hasciicam_virtual_camera_device *device,
                           const hasciicam_virtual_camera_frame *frame) {
    hasciicam_virtual_camera_windows_state *state;
    hasciicam_virtual_camera_pipe_frame header;
    DWORD bytes_written = 0;

    state = (hasciicam_virtual_camera_windows_state *)
        hasciicam_virtual_camera_device_state(device);
    if (state == NULL || frame == NULL)
        return 0;
    if (InterlockedCompareExchange(&state->connected, 1, 1) != 1)
        return 1;
    if (state->message_buffer == NULL || state->message_size == 0)
        return 0;
    if (frame->pixel_format != HASCIICAM_VIRTUAL_CAMERA_PIXFMT_BGRA32 || frame->pixels == NULL)
        return 0;

    hasciicam_virtual_camera_pipe_frame_init(&header,
                                             HASCIICAM_VIRTUAL_CAMERA_PIXFMT_YUY2,
                                             state->request.width,
                                             state->request.height,
                                             state->request.width * 2,
                                             state->sequence++,
                                             frame->timestamp_100ns);
    if (!hasciicam_virtual_camera_scale_bgra32_to_yuy2(
            frame->pixels,
            frame->width,
            frame->height,
            frame->stride_bytes,
            state->message_buffer + sizeof(header),
            state->request.width,
            state->request.height,
            state->request.width * 2,
            0,
            0))
        return 0;
    if (!hasciicam_virtual_camera_pipe_encode_message(
            &header,
            state->message_buffer + sizeof(header),
            state->payload_size,
            state->message_buffer,
            state->message_size,
            NULL,
            0))
        return 0;
    if (!WriteFile(state->pipe_handle,
                   state->message_buffer,
                   (DWORD)state->message_size,
                   &bytes_written,
                   NULL)) {
        DWORD err = GetLastError();
        if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
            InterlockedExchange(&state->connected, 0);
    }
    return 1;
}

static void windows_close(hasciicam_virtual_camera_device *device) {
    hasciicam_virtual_camera_windows_state *state;

    state = (hasciicam_virtual_camera_windows_state *)
        hasciicam_virtual_camera_device_state(device);
    if (state == NULL)
        return;
    if (state->stop_event != NULL)
        SetEvent(state->stop_event);
    if (state->accept_thread != NULL)
        CancelSynchronousIo(state->accept_thread);
    if (state->accept_thread != NULL) {
        WaitForSingleObject(state->accept_thread, INFINITE);
        CloseHandle(state->accept_thread);
    }
    if (state->pipe_handle != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(state->pipe_handle);
        CloseHandle(state->pipe_handle);
    }
    if (state->stop_event != NULL)
        CloseHandle(state->stop_event);
    free(state->message_buffer);
    free(state);
}

const char *hasciicam_virtual_camera_windows_name(void) {
    return "windows-pipe";
}

static const hasciicam_virtual_camera_ops windows_ops = {
    windows_publish,
    windows_close,
    hasciicam_virtual_camera_windows_name
};

int hasciicam_virtual_camera_windows_open(hasciicam_virtual_camera_device **out,
                                          const hasciicam_virtual_camera_request *request,
                                          char *err,
                                          size_t err_size) {
    hasciicam_virtual_camera_windows_state *state;
    char local_err[128];

    if (out == NULL || request == NULL)
        return 0;
    *out = NULL;
    state = (hasciicam_virtual_camera_windows_state *)calloc(1, sizeof(*state));
    if (state == NULL) {
        hasciicam_virtual_camera_set_error(err, err_size, "unable to allocate Windows pipe state");
        return 0;
    }
    state->pipe_handle = INVALID_HANDLE_VALUE;
    state->request = *request;
    if (!hasciicam_virtual_camera_pipe_build_name(request,
                                                  state->pipe_name,
                                                  sizeof(state->pipe_name),
                                                  local_err,
                                                  sizeof(local_err))) {
        hasciicam_virtual_camera_set_error(err, err_size, local_err);
        free(state);
        return 0;
    }
    state->payload_size = hasciicam_virtual_camera_yuy2_size(
        request->width, request->height, request->width * 2);
    state->message_size = sizeof(hasciicam_virtual_camera_pipe_frame) + state->payload_size;
    state->message_buffer = (unsigned char *)calloc(1, state->message_size);
    if (state->message_buffer == NULL) {
        hasciicam_virtual_camera_set_error(
            err, err_size, "unable to allocate virtual camera message buffer");
        free(state);
        return 0;
    }
    state->pipe_handle = CreateNamedPipeA(state->pipe_name,
                                          PIPE_ACCESS_OUTBOUND,
                                          PIPE_TYPE_BYTE | PIPE_WAIT,
                                          1,
                                          (DWORD)state->message_size,
                                          (DWORD)state->message_size,
                                          0,
                                          NULL);
    if (state->pipe_handle == INVALID_HANDLE_VALUE) {
        hasciicam_virtual_camera_set_error(err, err_size, "unable to create named pipe");
        free(state->message_buffer);
        free(state);
        return 0;
    }
    state->stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (state->stop_event == NULL) {
        hasciicam_virtual_camera_set_error(
            err, err_size, "unable to create virtual camera stop event");
        CloseHandle(state->pipe_handle);
        free(state->message_buffer);
        free(state);
        return 0;
    }
    state->accept_thread = CreateThread(NULL, 0, windows_accept_thread, state, 0, NULL);
    if (state->accept_thread == NULL) {
        hasciicam_virtual_camera_set_error(
            err, err_size, "unable to create virtual camera accept thread");
        CloseHandle(state->stop_event);
        CloseHandle(state->pipe_handle);
        free(state->message_buffer);
        free(state);
        return 0;
    }

    *out = hasciicam_virtual_camera_device_create(
        &windows_ops, 1, hasciicam_virtual_camera_windows_name(), state, err, err_size);
    if (*out == NULL) {
        SetEvent(state->stop_event);
        CancelSynchronousIo(state->accept_thread);
        WaitForSingleObject(state->accept_thread, INFINITE);
        CloseHandle(state->accept_thread);
        CloseHandle(state->stop_event);
        CloseHandle(state->pipe_handle);
        free(state->message_buffer);
        free(state);
        return 0;
    }
    return 1;
}
