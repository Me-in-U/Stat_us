package com.meinu.status.api.ingest.entity;

import com.meinu.status.api.member.entity.Member;
import jakarta.persistence.*;
import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Getter;
import lombok.NoArgsConstructor;
import org.hibernate.annotations.CreationTimestamp;

import java.time.Instant;

@Getter
@Builder
@NoArgsConstructor
@AllArgsConstructor
@Entity
@Table(name = "vscode_events", indexes = {
        @Index(name = "idx_vscode_events_member", columnList = "member_id"),
        @Index(name = "idx_vscode_events_created", columnList = "created_at")
})
public class VscodeEvent {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "member_id", nullable = false)
    private Member member;

    // Use LONGTEXT to avoid truncation for large payloads on MySQL
    @Lob
    @Column(name = "payload", nullable = false, columnDefinition = "LONGTEXT")
    private String payload; // raw JSON string for flexibility

    @CreationTimestamp
    @Column(name = "created_at", updatable = false)
    private Instant createdAt;
}
