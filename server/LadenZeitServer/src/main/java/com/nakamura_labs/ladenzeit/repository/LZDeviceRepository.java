package com.nakamura_labs.ladenzeit.repository;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import com.nakamura_labs.ladenzeit.entity.LZDevice;

@Repository
public interface LZDeviceRepository extends JpaRepository<LZDevice, String> {
	LZDevice findBySessionOtp(String sessionOtp);

	LZDevice findBySessionCookie(String sessionCookie);

}
