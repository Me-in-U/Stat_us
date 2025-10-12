package com.meinu.status.api.status.controller;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import com.meinu.status.api.member.entity.Member;
import com.meinu.status.api.member.service.MemberService;
import com.meinu.status.api.status.sse.SseEmitterRegistry;
import org.springframework.http.MediaType;
import org.springframework.security.core.Authentication;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

@RestController
@RequestMapping("/api/status")
public class StatusSseController {
    private static final Logger log = LoggerFactory.getLogger(StatusSseController.class);
    private final SseEmitterRegistry registry;
    private final MemberService memberService;

    public StatusSseController(SseEmitterRegistry registry, MemberService memberService) {
        this.registry = registry;
        this.memberService = memberService;
    }

    @GetMapping(path = "/stream", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
    public SseEmitter stream(Authentication auth) {
        Member me = memberService.getByEmail(auth.getName());
        if (log.isInfoEnabled()) {
            log.info("SSE connect: memberId={} email={}", me.getId(), me.getEmail());
        }
        // 30분 타임아웃
        return registry.register(me.getId(), 30 * 60 * 1000L);
    }
}
