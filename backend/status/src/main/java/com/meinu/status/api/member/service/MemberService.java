package com.meinu.status.api.member.service;

import com.meinu.status.api.member.entity.Member;
import com.meinu.status.api.member.repository.MemberRepository;
import com.meinu.status.global.common.base.BaseException;
import com.meinu.status.global.common.base.BaseResponseStatus;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.security.SecureRandom;
import java.util.Base64;
import java.util.Set;

@Service
@Transactional
public class MemberService {
    private final MemberRepository memberRepository;
    private final PasswordEncoder passwordEncoder;

    public MemberService(MemberRepository memberRepository, PasswordEncoder passwordEncoder) {
        this.memberRepository = memberRepository;
        this.passwordEncoder = passwordEncoder;
    }

    public Member signup(String email, String rawPassword, String nickname) {
        if (memberRepository.existsByEmail(email))
            throw new BaseException(BaseResponseStatus.EMAIL_ALREADY_EXISTS);
        String hash = passwordEncoder.encode(rawPassword);
        Member m = Member.builder()
                .email(email)
                .passwordHash(hash)
                .nickname(nickname)
                .roles(Set.of("ROLE_USER"))
                .build();
        return memberRepository.save(m);
    }

    @Transactional(readOnly = true)
    public Member getByEmail(String email) {
        return memberRepository.findByEmail(email)
                .orElseThrow(() -> new BaseException(BaseResponseStatus.MEMBER_NOT_FOUND));
    }

    @Transactional(readOnly = true)
    public boolean matches(String rawPassword, String hash) {
        return passwordEncoder.matches(rawPassword, hash);
    }

    public String issueNewApiKey(Member member) {
        // generate 32-byte random key and Base64 URL encode (no padding)
        byte[] bytes = new byte[32];
        new SecureRandom().nextBytes(bytes);
        String key = Base64.getUrlEncoder().withoutPadding().encodeToString(bytes);
        member.setApiKey(key);
        memberRepository.save(member);
        return key;
    }
}
