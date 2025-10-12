package com.meinu.status.api.ingest.controller;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.meinu.status.api.ingest.entity.VscodeEvent;
import com.meinu.status.api.ingest.repository.VscodeEventRepository;
import com.meinu.status.api.member.entity.Member;
import com.meinu.status.api.member.repository.MemberRepository;
import com.meinu.status.global.common.base.BaseResponse;
import com.meinu.status.global.common.base.BaseResponseStatus;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.ResponseEntity;
import org.springframework.data.redis.core.StringRedisTemplate;
import java.time.Duration;
import java.time.LocalDate;
import com.meinu.status.api.status.sse.SseEmitterRegistry;
import org.springframework.util.StringUtils;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

@RestController
@RequestMapping("/api/ingest")
public class VscodeIngestController {
    private static final Logger log = LoggerFactory.getLogger(VscodeIngestController.class);
    private final MemberRepository memberRepository;
    private final VscodeEventRepository eventRepository;
    private final ObjectMapper objectMapper;
    private final StringRedisTemplate redis;
    private final SseEmitterRegistry sseRegistry;

    @Value("${app.ingest.snapshot-ttl-seconds:86400}")
    private long snapshotTtlSeconds;

    public VscodeIngestController(MemberRepository memberRepository, VscodeEventRepository eventRepository,
            ObjectMapper objectMapper, StringRedisTemplate redis, SseEmitterRegistry sseRegistry) {
        this.memberRepository = memberRepository;
        this.eventRepository = eventRepository;
        this.objectMapper = objectMapper;
        this.redis = redis;
        this.sseRegistry = sseRegistry;
    }

    @PostMapping("/vscode")
    public ResponseEntity<BaseResponse<Map<String, Object>>> accept(
            @RequestHeader(value = "x-api-key", required = false) String headerKey,
            @RequestBody Map<String, Object> body) {
        if (!StringUtils.hasText(headerKey)) {
            return ResponseEntity.status(401).body(BaseResponse.of(BaseResponseStatus.API_KEY_REQUIRED));
        }
        Member member = memberRepository.findByApiKey(headerKey)
                .orElse(null);
        if (member == null) {
            return ResponseEntity.status(401).body(BaseResponse.of(BaseResponseStatus.API_KEY_INVALID));
        }
        String json;
        try {
            json = objectMapper.writeValueAsString(body);
        } catch (JsonProcessingException e) {
            // fallback to toString
            json = body.toString();
        }
        VscodeEvent event = VscodeEvent.builder()
                .member(member)
                .payload(json)
                .build();
        eventRepository.save(event);
        if (log.isInfoEnabled()) {
            log.info("Ingest: memberId={} email={} payloadSize={} bytes", member.getId(), member.getEmail(),
                    json.length());
        }

        // Write latest snapshot and daily counters into Redis (fast path)
        try {
            String snapshotKey = "status:latest:" + member.getId();
            redis.opsForValue().set(snapshotKey, json, Duration.ofSeconds(snapshotTtlSeconds));

            // Increment lightweight metrics if present
            Object ks = body.get("keystrokes");
            long incKs = 0L;
            if (ks instanceof Number n)
                incKs = n.longValue();
            Object activeMs = body.get("sessionActiveMs");
            long incActive = 0L;
            if (activeMs instanceof Number n2)
                incActive = n2.longValue();
            String day = LocalDate.now().toString();
            if (incKs > 0) {
                String kKey = String.format("metrics:keystrokes:%d:%s", member.getId(), day);
                redis.opsForValue().increment(kKey, incKs);
            }
            if (incActive > 0) {
                String aKey = String.format("metrics:activeMs:%d:%s", member.getId(), day);
                redis.opsForValue().increment(aKey, incActive);
            }
        } catch (Exception ignored) {
            // Redis optional path: don't fail ingest if Redis is unavailable
        }
        // Notify SSE subscribers (non-blocking best-effort)
        try {
            sseRegistry.send(member.getId(), body);
            if (log.isDebugEnabled()) {
                log.debug("Ingest: SSE broadcast queued for memberId={}", member.getId());
            }
        } catch (Exception ignored) {
            // ignore: SSE is best-effort; if client disconnected, nothing else to do
        }
        return ResponseEntity.ok(BaseResponse.success(body));
    }
}
