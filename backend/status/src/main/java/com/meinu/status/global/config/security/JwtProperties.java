package com.meinu.status.global.config.security;

import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.context.annotation.Configuration;

@Configuration
@ConfigurationProperties(prefix = "app.jwt")
public class JwtProperties {
    private String secret;
    private long accessExpSeconds = 900; // 15m
    private long refreshExpSeconds = 60L * 60 * 24 * 14; // 14d
    private String issuer = "stat-us";

    public String getSecret() {
        return secret;
    }

    public void setSecret(String secret) {
        this.secret = secret;
    }

    public long getAccessExpSeconds() {
        return accessExpSeconds;
    }

    public void setAccessExpSeconds(long accessExpSeconds) {
        this.accessExpSeconds = accessExpSeconds;
    }

    public long getRefreshExpSeconds() {
        return refreshExpSeconds;
    }

    public void setRefreshExpSeconds(long refreshExpSeconds) {
        this.refreshExpSeconds = refreshExpSeconds;
    }

    public String getIssuer() {
        return issuer;
    }

    public void setIssuer(String issuer) {
        this.issuer = issuer;
    }
}
