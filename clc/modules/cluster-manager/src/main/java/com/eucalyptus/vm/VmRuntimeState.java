/*******************************************************************************
 * Copyright (c) 2009  Eucalyptus Systems, Inc.
 * 
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, only version 3 of the License.
 * 
 * 
 *  This file is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 * 
 *  You should have received a copy of the GNU General Public License along
 *  with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *  Please contact Eucalyptus Systems, Inc., 130 Castilian
 *  Dr., Goleta, CA 93101 USA or visit <http://www.eucalyptus.com/licenses/>
 *  if you need additional information or have any questions.
 * 
 *  This file may incorporate work covered under the following copyright and
 *  permission notice:
 * 
 *    Software License Agreement (BSD License)
 * 
 *    Copyright (c) 2008, Regents of the University of California
 *    All rights reserved.
 * 
 *    Redistribution and use of this software in source and binary forms, with
 *    or without modification, are permitted provided that the following
 *    conditions are met:
 * 
 *      Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 * 
 *      Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 * 
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 *    IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 *    OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. USERS OF
 *    THIS SOFTWARE ACKNOWLEDGE THE POSSIBLE PRESENCE OF OTHER OPEN SOURCE
 *    LICENSED MATERIAL, COPYRIGHTED MATERIAL OR PATENTED MATERIAL IN THIS
 *    SOFTWARE, AND IF ANY SUCH MATERIAL IS DISCOVERED THE PARTY DISCOVERING
 *    IT MAY INFORM DR. RICH WOLSKI AT THE UNIVERSITY OF CALIFORNIA, SANTA
 *    BARBARA WHO WILL THEN ASCERTAIN THE MOST APPROPRIATE REMEDY, WHICH IN
 *    THE REGENTS' DISCRETION MAY INCLUDE, WITHOUT LIMITATION, REPLACEMENT
 *    OF THE CODE SO IDENTIFIED, LICENSING OF THE CODE SO IDENTIFIED, OR
 *    WITHDRAWAL OF THE CODE CAPABILITY TO THE EXTENT NEEDED TO COMPLY WITH
 *    ANY SUCH LICENSES OR RIGHTS.
 *******************************************************************************
 * @author chris grzegorczyk <grze@eucalyptus.com>
 */

package com.eucalyptus.vm;

import java.util.Date;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import javax.persistence.CollectionTable;
import javax.persistence.Column;
import javax.persistence.ElementCollection;
import javax.persistence.Embeddable;
import javax.persistence.Embedded;
import javax.persistence.EnumType;
import javax.persistence.Enumerated;
import javax.persistence.Lob;
import javax.persistence.Transient;
import org.apache.log4j.Logger;
import org.hibernate.annotations.Cache;
import org.hibernate.annotations.CacheConcurrencyStrategy;
import org.hibernate.annotations.Parent;
import com.eucalyptus.component.id.Eucalyptus;
import com.eucalyptus.records.EventRecord;
import com.eucalyptus.records.EventType;
import com.eucalyptus.records.Logs;
import com.eucalyptus.system.Threads;
import com.eucalyptus.vm.VmBundleTask.BundleState;
import com.eucalyptus.vm.VmInstance.Reason;
import com.eucalyptus.vm.VmInstance.VmState;
import com.eucalyptus.vm.VmInstance.VmStateSet;
import com.google.common.collect.Sets;

@Embeddable
public class VmRuntimeState {
  @Transient
  private static String     SEND_USER_TERMINATE = "SIGTERM";
  @Transient
  private static String     SEND_USER_STOP      = "SIGSTOP";
  @Transient
  private static Logger     LOG                 = Logger.getLogger( VmRuntimeState.class );
  @Parent
  private VmInstance        vmInstance;
  @Embedded
  private VmBundleTask      bundleTask;
  @Embedded
  private VmCreateImageTask createImageTask;
  @Column( name = "metadata_vm_service_tag" )
  private String            serviceTag;
  @Enumerated( EnumType.STRING )
  @Column( name = "metadata_vm_reason" )
  private Reason            reason;
  @ElementCollection
  @CollectionTable( name = "metadata_instances_state_reasons" )
  @Cache( usage = CacheConcurrencyStrategy.TRANSACTIONAL )
  private Set<String>       reasonDetails       = Sets.newHashSet( );
  @Transient
  private StringBuffer      consoleOutput       = new StringBuffer( );
  @Lob
  @Column( name = "metadata_vm_password_data" )
  private String            passwordData;
  @Column( name = "metadata_vm_pending" )
  private Boolean           pending;
  
  VmRuntimeState( final VmInstance vmInstance ) {
    super( );
    this.vmInstance = vmInstance;
  }
  
  VmRuntimeState( ) {
    super( );
  }
  
  public String getReason( ) {
    if ( this.reason == null ) {
      this.reason = Reason.NORMAL;
    }
    return this.reason.name( ) + ": " + this.reason + ( this.reasonDetails != null
      ? " -- " + this.reasonDetails
      : "" );
  }
  
  private void addReasonDetail( final String... extra ) {
    for ( final String s : extra ) {
      this.reasonDetails.add( s );
    }
  }
  
  public void setState( final VmState newState, Reason reason, final String... extra ) {
    final VmState oldState = this.getVmInstance( ).getState( );
    Runnable action = null;
    if ( VmState.SHUTTING_DOWN.equals( newState ) && VmState.SHUTTING_DOWN.equals( oldState ) && Reason.USER_TERMINATED.equals( reason ) ) {
      action = this.cleanUpRunnable( SEND_USER_TERMINATE );
    } else if ( VmState.STOPPING.equals( newState ) && VmState.STOPPING.equals( oldState ) && Reason.USER_STOPPED.equals( reason ) ) {
      action = this.cleanUpRunnable( SEND_USER_STOP );
    } else if ( ( VmState.TERMINATED.equals( newState ) && VmState.TERMINATED.equals( oldState ) ) || VmState.BURIED.equals( newState ) ) {
      action = this.cleanUpRunnable( SEND_USER_TERMINATE );
    } else if ( !oldState.equals( newState ) ) {
      if ( Reason.APPEND.equals( reason ) ) {
        reason = this.reason;
      }
      this.addReasonDetail( extra );
      LOG.info( String.format( "%s state change: %s -> %s", this.getVmInstance( ).getInstanceId( ), this.getVmInstance( ).getState( ), newState ) );
      this.reason = reason;
      if ( VmStateSet.RUN.contains( oldState ) && VmStateSet.NOT_RUNNING.contains( newState ) ) {
        this.getVmInstance( ).setState( newState );
        action = this.cleanUpRunnable( );
      } else if ( VmState.PENDING.equals( oldState ) && VmState.RUNNING.equals( newState ) ) {
        this.getVmInstance( ).setState( newState );
      } else if ( VmState.TERMINATED.equals( newState ) && ( oldState.ordinal( ) <= VmState.RUNNING.ordinal( ) ) ) {
        this.getVmInstance( ).setState( newState );
        action = this.cleanUpRunnable( );
      } else if ( VmState.TERMINATED.equals( newState ) && ( oldState.ordinal( ) > VmState.RUNNING.ordinal( ) ) ) {
        this.getVmInstance( ).setState( newState );
//      } else if ( ( oldState.ordinal( ) > VmState.RUNNING.ordinal( ) ) && ( newState.ordinal( ) <= VmState.RUNNING.ordinal( ) ) ) {
//        this.getVmInstance( ).setState( oldState );
//        action = this.cleanUpRunnable( );
      } else if ( newState.ordinal( ) > oldState.ordinal( ) ) {
        this.getVmInstance( ).setState( newState );
      } else if ( VmState.STOPPED.equals( oldState ) && VmState.PENDING.equals( newState ) ) {
        this.getVmInstance( ).setState( newState );
      }
      try {
        this.getVmInstance( ).store( );
      } catch ( final Exception ex1 ) {
        LOG.error( ex1, ex1 );
      }
      if ( action != null ) {
        try {
          Threads.lookup( Eucalyptus.class, VmInstance.class ).limitTo( VmInstances.MAX_STATE_THREADS ).submit( action ).get( 10, TimeUnit.MILLISECONDS );
        } catch ( final TimeoutException ex ) {
        } catch ( final Exception ex ) {
          LOG.error( ex, ex );
        }
      }
      if ( !this.getVmInstance( ).getState( ).equals( oldState ) ) {
        EventRecord.caller( VmInstance.class, EventType.VM_STATE, this.getVmInstance( ).getInstanceId( ), this.vmInstance.getOwner( ),
                              this.getVmInstance( ).getState( ).name( ) );
      }
    }
  }
  
  private Runnable cleanUpRunnable( ) {
    return this.cleanUpRunnable( null );
  }
  
  private Runnable cleanUpRunnable( final String reason ) {
    return new Runnable( ) {
      @Override
      public void run( ) {
        VmInstances.cleanUp( VmRuntimeState.this.getVmInstance( ) );
        if ( ( reason != null ) && !VmRuntimeState.this.reasonDetails.contains( reason ) ) {
          VmRuntimeState.this.addReasonDetail( reason );
        }
      }
    };
  }
  
  VmBundleTask resetBundleTask( ) {
    final VmBundleTask oldTask = this.bundleTask;
    this.bundleTask = null;
    return oldTask;
  }
  
  String getServiceTag( ) {
    return this.serviceTag;
  }
  
  void setServiceTag( final String serviceTag ) {
    this.serviceTag = serviceTag;
  }
  
  void setReason( final Reason reason ) {
    this.reason = reason;
  }
  
  StringBuffer getConsoleOutput( ) {
    return this.consoleOutput;
  }
  
  void setConsoleOutput( final StringBuffer consoleOutput ) {
    this.consoleOutput = consoleOutput;
    if ( this.passwordData == null ) {
      final String tempCo = consoleOutput.toString( ).replaceAll( "[\r\n]*", "" );
      if ( tempCo.matches( ".*<Password>[\\w=+/]*</Password>.*" ) ) {
        this.passwordData = tempCo.replaceAll( ".*<Password>", "" ).replaceAll( "</Password>.*", "" );
      }
    }
  }
  
  String getPasswordData( ) {
    return this.passwordData;
  }
  
  void setPasswordData( final String passwordData ) {
    this.passwordData = passwordData;
  }
  
  VmInstance getVmInstance( ) {
    return this.vmInstance;
  }
  
  VmBundleTask getBundleTask( ) {
    return this.bundleTask;
  }
  
  /**
   * @return
   */
  public Boolean isBundling( ) {
    return this.bundleTask != null && !BundleState.none.equals( this.bundleTask.getState( ) );
  }
  
  BundleState getBundleTaskState( ) {
    if ( this.bundleTask != null ) {
      return this.getBundleTask( ).getState( );
    } else {
      return BundleState.none;
    }
  }
  
  public Boolean cancelBundleTask( ) {
    if ( this.getBundleTask( ) != null ) {
      this.getBundleTask( ).setState( BundleState.canceling );
      EventRecord.here( VmRuntimeState.class, EventType.BUNDLE_CANCELING, this.vmInstance.getOwner( ).toString( ), this.getBundleTask( ).getBundleId( ),
                        this.getVmInstance( ).getInstanceId( ),
                        "" + this.getBundleTask( ).getState( ) ).info( );
      return true;
    } else {
      return false;
    }
  }
  
  public Boolean submittedBundleTask( ) {
    if ( this.getBundleTask( ) != null && this.getBundleTask( ).getState( ).ordinal( ) >= BundleState.storing.ordinal( ) ) {
      this.getBundleTask( ).setState( BundleState.storing );
      EventRecord.here( VmRuntimeState.class, EventType.BUNDLE_STARTING,
                        this.vmInstance.getOwner( ).toString( ),
                        this.getBundleTask( ).getBundleId( ),
                        this.getVmInstance( ).getInstanceId( ),
                        "" + this.getBundleTask( ).getState( ) ).info( );
      return true;
    } else if ( BundleState.canceling.name( ).equals( this.getBundleTaskState( ) ) ) {
      EventRecord.here( VmRuntimeState.class, EventType.BUNDLE_CANCELLED, this.vmInstance.getOwner( ).toString( ), this.getBundleTask( ).getBundleId( ),
                        this.getVmInstance( ).getInstanceId( ),
                        "" + this.getBundleTask( ).getState( ) ).info( );
      this.resetBundleTask( );
      return true;
    } else {
      return false;
    }
  }
  
  public Boolean startBundleTask( final VmBundleTask task ) {
    if ( !this.isBundling( ) ) {
      this.bundleTask = task;
      return true;
    } else {
      if ( ( this.getBundleTask( ) != null ) && ( BundleState.failed.equals( task ) || BundleState.canceling.equals( task ) ) ) {
        this.resetBundleTask( );
        this.bundleTask = task;
        return true;
      } else {
        return false;
      }
    }
  }
  
  private void setBundleTask( final VmBundleTask bundleTask ) {
    this.bundleTask = bundleTask;
  }
  
  private void setVmInstance( final VmInstance vmInstance ) {
    this.vmInstance = vmInstance;
  }
  
  public Boolean isCreatingImage( ) {
    return this.createImageTask != null;
  }
  
  /**
   * @param createImageTaskStateName
   */
  public void setCreateImageTaskState( final String createImageTaskStateName ) {
    if ( this.createImageTask != null ) {
      this.createImageTask.setState( createImageTaskStateName );
    }
  }
  
  @Override
  public String toString( ) {
    final StringBuilder builder = new StringBuilder( );
    builder.append( "VmRuntimeState:" );
    if ( this.bundleTask != null ) builder.append( "bundleTask=" ).append( this.bundleTask ).append( ":" );
    if ( this.createImageTask != null ) builder.append( "createImageTask=" ).append( this.createImageTask ).append( ":" );
    if ( this.serviceTag != null ) builder.append( "serviceTag=" ).append( this.serviceTag ).append( ":" );
    if ( this.reason != null ) builder.append( "reason=" ).append( this.reason ).append( ":" );
    if ( this.reasonDetails != null ) builder.append( "reasonDetails=" ).append( this.reasonDetails ).append( ":" );
    if ( this.consoleOutput != null ) builder.append( "consoleOutput=" ).append( this.consoleOutput ).append( ":" );
    if ( this.passwordData != null ) builder.append( "passwordData=" ).append( this.passwordData ).append( ":" );
    if ( this.pending != null ) builder.append( "pending=" ).append( this.pending );
    return builder.toString( );
  }
  
  private VmCreateImageTask getCreateImageTask( ) {
    return this.createImageTask;
  }
  
  private void setCreateImageTask( VmCreateImageTask createImageTask ) {
    this.createImageTask = createImageTask;
  }
  
  private Boolean getPending( ) {
    return this.pending;
  }
  
  private void setPending( Boolean pending ) {
    this.pending = pending;
  }
  
  private Set<String> getReasonDetails( ) {
    return this.reasonDetails;
  }
  
  private void setReasonDetails( Set<String> reasonDetails ) {
    this.reasonDetails = reasonDetails;
  }
  
  public void updateBundleTaskState( String state ) {
    BundleState next = BundleState.mapper.apply( state );
    updateBundleTaskState( next );
  }
  
  public void updateBundleTaskState( BundleState state ) {
    if ( this.getBundleTask( ) != null ) {
      final BundleState current = this.getBundleTask( ).getState( );
      if ( BundleState.complete.equals( state ) && !BundleState.complete.equals( current ) ) {
        this.getBundleTask( ).setState( state );
      } else if ( BundleState.failed.equals( state ) && !BundleState.failed.equals( current ) ) {
        this.getBundleTask( ).setState( state );
      } else if ( BundleState.canceling.equals( current ) || BundleState.canceling.equals( state ) ) {
      } else if ( BundleState.pending.equals( current ) && !BundleState.none.equals( state ) ) {
        this.getBundleTask( ).setState( state );
        this.getBundleTask( ).setUpdateTime( new Date( ) );
        EventRecord.here( VmRuntimeState.class, EventType.BUNDLE_TRANSITION, this.vmInstance.getOwner( ).toString( ), "" + this.getBundleTask( ) ).info( );
      } else if ( BundleState.storing.equals( state ) ) {
        this.getBundleTask( ).setState( state );
        this.getBundleTask( ).setUpdateTime( new Date( ) );
        EventRecord.here( VmRuntimeState.class, EventType.BUNDLE_TRANSITION, this.vmInstance.getOwner( ).toString( ), "" + this.getBundleTask( ) ).info( );
      }
    } else {
      this.setBundleTask( new VmBundleTask( this.vmInstance, state.name( ), new Date( ), new Date( ), 0, "unknown", "unknown", "unknown", "unknown" ) );
      Logs.extreme( ).trace( "Unhandle bundle task state update: " + state );
    }
  }
  
  @Override
  public int hashCode( ) {
    final int prime = 31;
    int result = 1;
    result = prime * result + ( ( this.vmInstance == null )
      ? 0
      : this.vmInstance.hashCode( ) );
    return result;
  }
  
  @Override
  public boolean equals( Object obj ) {
    if ( this == obj ) {
      return true;
    }
    if ( obj == null ) {
      return false;
    }
    if ( getClass( ) != obj.getClass( ) ) {
      return false;
    }
    VmRuntimeState other = ( VmRuntimeState ) obj;
    if ( this.vmInstance == null ) {
      if ( other.vmInstance != null ) {
        return false;
      }
    } else if ( !this.vmInstance.equals( other.vmInstance ) ) {
      return false;
    }
    return true;
  }
  
}
