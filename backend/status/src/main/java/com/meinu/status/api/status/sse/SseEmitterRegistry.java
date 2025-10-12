package com.meinu.status.api.status.sse;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.MediaType;
import org.springframework.stereotype.Component;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.io.IOException;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;

@Component
public class SseEmitterRegistry {
    private static final Logger log = LoggerFactory.getLogger(SseEmitterRegistry.class);
    private final Map<Long, List<SseEmitter>> emittersByMember = new ConcurrentHashMap<>();

    public SseEmitter register(Long memberId, Long timeoutMs) {
        SseEmitter emitter = new SseEmitter(timeoutMs);
        emittersByMember.computeIfAbsent(memberId, k -> new CopyOnWriteArrayList<>()).add(emitter);
        emitter.onCompletion(() -> remove(memberId, emitter));
        emitter.onTimeout(() -> remove(memberId, emitter));
        emitter.onError(e -> remove(memberId, emitter));
        if (log.isInfoEnabled()) {
            log.info("SSE register: memberId={} totalEmitters={}", memberId, emittersByMember.get(memberId).size());
        }
        // Send a lightweight initial event so clients know the stream is established.
        try {
            emitter.send(SseEmitter.event().name("ping").data("ok", MediaType.TEXT_PLAIN));
        } catch (IOException ignored) {
            // ignore: connection may close immediately; lifecycle hooks will remove it
        }
        return emitter;
    }

    public void send(Long memberId, Object data) {
        List<SseEmitter> list = emittersByMember.get(memberId);
        if (list == null)
            return;
        if (log.isDebugEnabled()) {
            log.debug("SSE send: memberId={} receivers={}", memberId, list.size());
        }
        for (SseEmitter em : list) {
            try {
                em.send(SseEmitter.event().name("status").data(data, MediaType.APPLICATION_JSON));
            } catch (IOException e) {
                // drop broken emitter
                em.completeWithError(e);
            }
        }
    }

    private void remove(Long memberId, SseEmitter emitter) {
        List<SseEmitter> list = emittersByMember.get(memberId);
        if (list != null) {
            list.remove(emitter);
            if (log.isInfoEnabled()) {
                log.info("SSE remove: memberId={} remainingEmitters={}", memberId, list.size());
            }
            if (list.isEmpty())
                emittersByMember.remove(memberId);
        }
    }
}
