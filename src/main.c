// main.c - الخطوة 1: اختبار قراءة يد السوني فور داخل تطبيق PKG
// يعرض إشعار أعلى الشاشة عند الضغط على أي زر، ويكتب نفس الشي في klog.
// (غير مجرّب: الكود مكتوب بدون إمكانية بناءه هنا، توقع أخطاء في أول بناء)

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>

// هيكل طلب الإشعار (معروف في مجتمع الهومبرو)
typedef struct {
    char useless1[45];
    char message[3075];
} notify_request_t;

// نستخدم اسم محلي مختلف + asm label عشان ما يتعارض مع أي تعريف في الهيدرات
int notify_syscall(int device, void *req, size_t size, int blocking)
    __asm__("sceKernelSendNotificationRequest");

static void notify(const char *msg)
{
    notify_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.message, msg, sizeof(req.message) - 1);
    notify_syscall(0, &req, sizeof(req), 0);
    printf("[XboxCtrl] %s\n", msg);
}

typedef struct {
    uint32_t mask;
    const char *name;
} button_name_t;

static const button_name_t BUTTONS[] = {
    { ORBIS_PAD_BUTTON_CROSS,    "Cross (X)"   },
    { ORBIS_PAD_BUTTON_CIRCLE,   "Circle"      },
    { ORBIS_PAD_BUTTON_SQUARE,   "Square"      },
    { ORBIS_PAD_BUTTON_TRIANGLE, "Triangle"    },
    { ORBIS_PAD_BUTTON_L1,       "L1"          },
    { ORBIS_PAD_BUTTON_R1,       "R1"          },
    { ORBIS_PAD_BUTTON_L2,       "L2"          },
    { ORBIS_PAD_BUTTON_R2,       "R2"          },
    { ORBIS_PAD_BUTTON_OPTIONS,  "Options"     },
    { ORBIS_PAD_BUTTON_UP,       "D-Pad Up"    },
    { ORBIS_PAD_BUTTON_DOWN,     "D-Pad Down"  },
    { ORBIS_PAD_BUTTON_LEFT,     "D-Pad Left"  },
    { ORBIS_PAD_BUTTON_RIGHT,    "D-Pad Right" },
};

int main(void)
{
    notify("Xbox Controller Test: started");

    sceUserServiceInitialize(NULL);

    int userId = 0;
    int ret = sceUserServiceGetInitialUser(&userId);
    if (ret != 0) {
        char b[96];
        snprintf(b, sizeof(b), "GetInitialUser failed: 0x%08X", (unsigned)ret);
        notify(b);
    }

    scePadInit();
    int pad = scePadOpen(userId, ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL);
    if (pad < 0) {
        char b[96];
        snprintf(b, sizeof(b), "scePadOpen failed: 0x%08X", (unsigned)pad);
        notify(b);
        // نكمل الحلقة عشان التطبيق ما يقفل فجأة
    } else {
        notify("Controller opened OK. Press any button");
    }

    uint32_t prev = 0;
    int stickReported = 0;

    for (;;) {
        if (pad >= 0) {
            OrbisPadData data;
            memset(&data, 0, sizeof(data));

            if (scePadReadState(pad, &data) == 0) {
                uint32_t pressed = data.buttons & ~prev;
                prev = data.buttons;

                for (size_t i = 0; i < sizeof(BUTTONS) / sizeof(BUTTONS[0]); i++) {
                    if (pressed & BUTTONS[i].mask) {
                        notify(BUTTONS[i].name);
                    }
                }

                // نبلّغ مرة وحدة بس لما تتحرك العصا اليسار (للتأكد إن الأنالوج يقرأ)
                int dx = (int)data.leftStick.x - 128;
                int dy = (int)data.leftStick.y - 128;
                if (!stickReported && (dx > 80 || dx < -80 || dy > 80 || dy < -80)) {
                    char b[64];
                    snprintf(b, sizeof(b), "Left stick: x=%d y=%d", data.leftStick.x, data.leftStick.y);
                    notify(b);
                    stickReported = 1;
                }
            }
        }
        sceKernelUsleep(16000); // ~60 مرة بالثانية
    }

    return 0;
}
